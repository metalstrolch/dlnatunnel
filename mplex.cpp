/*
 * dlnatunnel
 * Copyright (C) 2023 Stefan Wildemann
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "mplex.h"
#include <sys/socket.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
//#define DEBUG
#include "debugprintf.h"
#include <errno.h>

constexpr uint32_t mplex_frame_header_size() {
    mplex_frame * f{nullptr};
    return sizeof(*f) - sizeof(f->payload);
}

static inline uint32_t mplex_frame_size(const mplex_frame* const frame) {
    return (sizeof (*frame) - sizeof(frame->payload) + frame->payload_size);
}

static void on_choke_nop(mplex *, uint32_t, bool) {
    return;
}

mplex::mplex(socketmultiplex* mx, int socket, std::function<void(mplex* mpx)> on_ready,
             std::function<bool(mplex * mpx, uint32_t channel, void* reason, uint8_t size)> on_connect):
    m_mx{mx},
    m_socket{socket},
    m_ready{false},
    m_buffered{0},
    m_free_channel{1},
    m_attempts{},
    m_channels{},
    m_endpoints{},
    m_on_ready{on_ready},
    m_on_connect{on_connect} {
    bzero(&m_buffer, sizeof(m_buffer));
    send_hello();
}

mplex::~mplex() {
    debugprintf("MPLEX died");
}

int mplex::open_channel(std::function<bool(mplex * mpx, uint32_t channel)> f, void * reason, uint8_t size) {
    mplex_listener_helper h;
    h.channel=m_free_channel;
    debugprintf("Open channel %d", h.channel);
    m_free_channel ++;
    h.f = f;
    send_open(h.channel, reason, size);
    m_attempts.push_back(h);
    return h.channel;
}

void mplex::remove_attempt(uint32_t channel) {
    debugprintf("remove mpx attempt %d", channel);
    if(m_attempts.size() == 0)
        return;
    m_attempts.erase(std::remove_if(m_attempts.begin(), m_attempts.end(), [channel](mplex_listener_helper &h) {
        if(channel == h.channel) {
            return true;
        }
        return false;
    }), m_attempts.end());
}

int mplex::add_channel_listener(uint32_t channel, std::function<bool(mplex * mpx, mplex_frame * frame)> f) {
    debugprintf("Add mpx channel %d", channel);
    mplex_channel_helper h;
    h.channel=channel;
    h.f = f;
    h.onChoke = on_choke_nop;
    m_channels.push_back(h);
    return h.channel;
}

void mplex::remove_channel_listener(uint32_t channel) {
    debugprintf("remove mpx channel %d", channel);
    if(m_channels.size() == 0)
        return;
    m_channels.erase(std::remove_if(m_channels.begin(), m_channels.end(), [channel, this](mplex_channel_helper &h) {
        if(channel == h.channel) {
            send_close(channel);
            return true;
        }
        return false;
    }),m_channels.end());
}

int mplex::add_endpoint_listener(uint32_t channel, std::function<bool(mplex * mpx, mplex_frame * frame)> f) {
    debugprintf("Add mpx endpoint for channel %d", channel);
    mplex_channel_helper h;
    h.channel=channel;
    h.f = f;
    h.onChoke = on_choke_nop;
    m_endpoints.push_back(h);
    send_open_response(channel, false);
    return h.channel;
}

void mplex::remove_endpoint_listener(uint32_t channel) {
    debugprintf("remove mpx endpoint channel %d", channel);
    if(m_endpoints.size() == 0)
        return;
    m_endpoints.erase(std::remove_if(m_endpoints.begin(), m_endpoints.end(), [channel,this](mplex_channel_helper &h) {
        if(channel == h.channel) {
            send_close_response(channel);
            return true;
        }
        return false;
    }),m_endpoints.end());
}

void mplex::add_channel_choke(uint32_t channel,
                              std::function<void(mplex * mpx, uint32_t channel, bool enabled)> onChoke) {
    for(auto& helper: m_channels) {
        if(helper.channel == channel) {
            helper.onChoke = onChoke;
        }
    }
}
void mplex::add_endpoint_choke(uint32_t channel,
                               std::function<void(mplex * mpx, uint32_t channel, bool enabled)> onChoke) {
    for(auto& helper: m_endpoints) {
        if(helper.channel == channel) {
            helper.onChoke = onChoke;
        }
    }
}

void mplex::close_all() {
    std::vector<uint32_t> closing{};
    for(auto& helper: m_channels) {
        closing.push_back(helper.channel);
        helper.f(this, nullptr);
    }
    for(auto& channel: closing) {
        remove_channel_listener(channel);
    }

    closing.clear();
    for(auto& helper: m_endpoints) {
        closing.push_back(helper.channel);
        helper.f(this, nullptr);
    }
    for(auto& channel: closing) {
        remove_endpoint_listener(channel);
    }

    closing.clear();
    for(auto& helper: m_attempts) {
        closing.push_back(helper.channel);
        helper.f(this, 0);
    }
    for(auto& channel: closing) {
        remove_attempt(channel);
    }
}

bool mplex::receive(int rq_socket) {
    int n;
    if(rq_socket != m_socket) {
        errorprintf("Socket mismatch %d %d", rq_socket, m_socket);
        return true;
    }
    errno = 0;
    n = read(m_socket, ((uint8_t*)&m_buffer) + m_buffered, sizeof(m_buffer) - m_buffered);
    if((n < 0) || ((n == 0) && (errno != EINPROGRESS))) {
        //Socket died. Tell the others wer'e closing.
        debugprintf("CONTROL SOCKET DIED. GOING DOWN n==%d, errno %s", n, strerror(errno));
        close_all();
        return false;
    } else if(n==0) {
        usleep(1000);
    } else {
        m_buffered +=n;
    }
    do {
        if((m_buffered < mplex_frame_header_size()) || (m_buffered < mplex_frame_size(&m_buffer))) {
            debugprintf("short read. Waiting for more");
            return true;
        }

        uint32_t rest = m_buffered - mplex_frame_size(&m_buffer); // do before processing.
        if(!process_frame(&m_buffer)) {
            return false;
        }

        //Shift rest of bytes to front
        for(uint32_t a = 0; a < rest; a ++) {
            ((uint8_t*) &m_buffer)[a] = ((uint8_t*) &m_buffer)[m_buffered- rest +a];
        }
        m_buffered = rest;
        //debugprintf("%d bytes left", m_buffered);
    } while(m_buffered >= mplex_frame_header_size());


    return true;
}

bool mplex::process_frame(mplex_frame* frame) {
    switch (frame->type) {
    case MPLEX_TYPE_HELLO:
        debugprintf("Got HELLO frame. Send answer");
        send_hello_response(frame);
        break;
    case MPLEX_TYPE_DATA: {
        debugprintf("Got DATA frame for CH %d", frame->channel);
        std::vector<uint32_t> remove{};
        for(auto& helper: m_endpoints) {
            if(helper.channel == frame->channel) {
                if(!helper.f(this, frame)) {
                    remove.push_back(helper.channel);
                }
            }
        }
        for(auto channel: remove) {
            remove_endpoint_listener(channel);
        }
    }
    break;
    case MPLEX_TYPE_DATA | MPLEX_TYPE_RESPONSE: {
        debugprintf("Got DATA response frame for CH %d", frame->channel);
        std::vector<uint32_t> remove{};
        for(auto& helper: m_channels) {
            if(helper.channel == frame->channel) {
                if(!helper.f(this, frame)) {
                    remove.push_back(helper.channel);
                }
            }
        }
        for(auto channel: remove) {
            remove_channel_listener(channel);
        }
    }
    break;
    case MPLEX_TYPE_HELLO | MPLEX_TYPE_RESPONSE:
        debugprintf("Got HELLO response. PEER is MPLEX");
        m_ready=true;
        m_on_ready(this);
        break;
    case MPLEX_TYPE_OPEN: {
        bool found{false};
        debugprintf("Remote asks to open channel %d", frame->channel);
        for(auto& helper: m_endpoints) {
            if(helper.channel==frame->channel) {
                found = true;
            }
        }
        if(found) {
            errorprintf("ERROR: channel already in use");
            send_open_response(frame->channel, true);
        } else {
            //call callback
            if(!m_on_connect(this, frame->channel, (void*)(frame->payload.open.reason), frame->payload.open.reason_size))
                send_open_response(frame->channel, true);
        }
        break;
    }
    case MPLEX_TYPE_OPEN | MPLEX_TYPE_RESPONSE: {
        //find channel in attempts
        std::vector<uint32_t> remove{};
        for(auto& helper: m_attempts) {
            if(helper.channel == frame->channel) {
                remove.push_back(helper.channel);
                if(frame->payload.open.failure) {
                    errorprintf("Remote rejects new channel %d", frame->channel);
                    helper.f(this, 0);
                } else {
                    debugprintf("Remote accepts new channel %d", frame->channel);
                    helper.f(this, helper.channel);
                }
            }
        }
        //Remove from attempts
        for(auto channel: remove) {
            remove_attempt(channel);
        }
    }
    break;
    case MPLEX_TYPE_CLOSE: {
        debugprintf("Remote asks to close endpoint %d", frame->channel);
        for(auto helper : m_endpoints) {
            if(helper.channel == frame->channel) {
                helper.f(this, nullptr);
            }
        }
        remove_endpoint_listener(frame->channel);
    }
    break;
    case MPLEX_TYPE_CLOSE | MPLEX_TYPE_RESPONSE: {
        debugprintf("Remote asks to close channel %d", frame->channel);
        for(auto helper : m_channels) {
            if(helper.channel == frame->channel) {
                helper.f(this, nullptr);
            }
        }
        remove_channel_listener(frame->channel);
    }
    break;
    case MPLEX_TYPE_CHOKE: {
        debugprintf("%schoke endpoint %d", (frame->payload.choke.enable? "": "un"), frame->channel);
        for(auto helper : m_endpoints) {
            if(helper.channel == frame->channel) {
                helper.onChoke(this, frame->channel, frame->payload.choke.enable);
            }
        }
    }
    break;
    case MPLEX_TYPE_CHOKE | MPLEX_TYPE_RESPONSE: {
        debugprintf("%schoke channel %d", (frame->payload.choke.enable? "": "un"), frame->channel);
        for(auto helper : m_channels) {
            if(helper.channel == frame->channel) {
                helper.onChoke(this, frame->channel, frame->payload.choke.enable);
            }
        }
    }
    break;
    }
    return true;
}

void mplex::send_hello() {
    mplex_frame frame;
    int n;
    frame.type=MPLEX_TYPE_HELLO;
    frame.payload_size = 0;
    n=m_mx->awrite(m_socket, &frame, mplex_frame_size(&frame),true);
    if(n != mplex_frame_size(&frame))
        errorprintf("ERROR sending hello");
}

void mplex::send_hello_response(mplex_frame* frame) {
    int n;
    frame->type=MPLEX_TYPE_HELLO | MPLEX_TYPE_RESPONSE;
    n=m_mx->awrite(m_socket, frame, mplex_frame_size(frame),true);
    if(n != mplex_frame_size(frame))
        errorprintf("ERROR sending hello response");
}

void mplex::send_choke(uint32_t channel, bool enable) {
    debugprintf("Send choke channel %d %d", channel, enable);
    mplex_frame frame;
    int n;
    frame.type=MPLEX_TYPE_CHOKE;
    frame.channel=channel;
    frame.payload_size = sizeof(frame.payload.choke);
    frame.payload.choke.enable=enable;
    n=m_mx->awrite(m_socket, &frame, mplex_frame_size(&frame),true);
    if(n != mplex_frame_size(&frame))
        errorprintf("ERROR sending choke");
}

void mplex::send_choke_response(uint32_t channel, bool enable) {
    debugprintf("Send choke response %d %d", channel, enable);
    mplex_frame frame;
    int n;
    frame.type=MPLEX_TYPE_CHOKE | MPLEX_TYPE_RESPONSE;
    frame.channel=channel;
    frame.payload_size = sizeof(frame.payload.choke);
    frame.payload.choke.enable=enable;
    n=m_mx->awrite(m_socket, &frame, mplex_frame_size(&frame),true);
    if(n != mplex_frame_size(&frame))
        errorprintf("ERROR sending choke");
}

void mplex::send_open(uint32_t channel, void* reason, uint8_t size) {
    mplex_frame frame;
    int n;
    frame.type=MPLEX_TYPE_OPEN;
    frame.channel=channel;
    frame.payload_size = sizeof(frame.payload.open);
    frame.payload.open.failure=false;
    memcpy(&(frame.payload.open.reason), reason, size);
    frame.payload.open.reason_size = size;
    n=m_mx->awrite(m_socket, &frame, mplex_frame_size(&frame),true);
    if(n != mplex_frame_size(&frame))
        errorprintf("ERROR sending open");
}

void mplex::send_open_response(uint32_t channel, bool failure) {
    mplex_frame frame;
    int n;
    frame.type=MPLEX_TYPE_OPEN | MPLEX_TYPE_RESPONSE;
    frame.channel=channel;
    frame.payload_size = sizeof(frame.payload.open);
    frame.payload.open.failure=failure;
    n=m_mx->awrite(m_socket, &frame, mplex_frame_size(&frame),true);
    if(n != mplex_frame_size(&frame))
        errorprintf("ERROR sending open response");
}
void mplex::send_close(uint32_t channel) {
    debugprintf("Send close on channel %d", channel);
    mplex_frame frame;
    int n;
    frame.type=MPLEX_TYPE_CLOSE;
    frame.channel=channel;
    frame.payload_size = 0;
    n=m_mx->awrite(m_socket, &frame, mplex_frame_size(&frame),true);
    if(n != mplex_frame_size(&frame))
        errorprintf("ERROR sending close");
}

void mplex::send_close_response(uint32_t channel) {
    debugprintf("Send close response on channel %d", channel);
    mplex_frame frame;
    int n;
    frame.type=MPLEX_TYPE_CLOSE | MPLEX_TYPE_RESPONSE;
    frame.channel=channel;
    frame.payload_size = 0;
    n=m_mx->awrite(m_socket, &frame, mplex_frame_size(&frame),true);
    if(n != mplex_frame_size(&frame))
        errorprintf("ERROR sending close response");
}

int mplex::send_data(uint32_t channel, mplex_frame* frame) {
    int n;
    frame->type=MPLEX_TYPE_DATA;
    frame->channel=channel;
    n=m_mx->awrite(m_socket, frame, mplex_frame_size(frame),true);
    if(n != mplex_frame_size(frame))
        errorprintf("ERROR sending data");
    return n;
}

int mplex::send_data_response(uint32_t channel, mplex_frame* frame) {
    int n;
    frame->type=MPLEX_TYPE_DATA | MPLEX_TYPE_RESPONSE;
    frame->channel=channel;
    n=m_mx->awrite(m_socket, frame, mplex_frame_size(frame),true);
    debugprintf("%d, %d", frame->payload_size, n);
    if(n != mplex_frame_size(frame))
        errorprintf("ERROR sending data response");
    return n;
}

