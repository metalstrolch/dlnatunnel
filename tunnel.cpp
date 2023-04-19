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

#include "tunnel.h"
#include <functional>
#include <stdio.h>
#include <string.h>
#include <memory>
#include "mplex.h"
#include "socketmultiplex.h"
#include "tunnel_filter.h"
//#define DEBUG
#include "debugprintf.h"

#define TUNNEL_CONNECT_REASON_FORWARD 1
#define TUNNEL_CONNECT_REASON_MCAST_FORWARD 2
#pragma pack(push,1)
struct tunnel_connect_reason {
    uint8_t reason{0};
    uint16_t port{0};
    char host[256 - 4] {};
};
#pragma pack(pop)

tunnel::tunnel(socketmultiplex * mx, int socket, std::function<void(tunnel* tn)> on_ready) :
    m_mplex{nullptr},
    m_mx{mx},
    m_socket{socket},
    m_on_ready{on_ready} {
};

tunnel::~tunnel() {
    debugprintf("TUNNEL died");
    if (m_mplex != nullptr)
        delete m_mplex;
}

void tunnel::run() {
    m_mplex = new mplex(m_mx, m_socket, [this](mplex* mpx) {
        return on_mplex_ready(mpx);
    }, [this] (mplex * mpx, uint32_t channel, void* reason, uint8_t size) {
        return on_mplex_connect(mpx, channel, reason, size);
    });
}

void tunnel::on_mplex_ready(mplex* mpx) {
    debugprintf( "MPLEX ready");
    return m_on_ready(this);
}

bool tunnel::on_mplex_connect(mplex * mpx, uint32_t channel, void* reason, uint8_t size) {
    debugprintf( "MPLEX wants to connect channel %d", channel);
    tunnel_connect_reason * r = (tunnel_connect_reason *) reason;
    if(size != sizeof(*r) || r == nullptr) {
        debugprintf( "Malformed reason");
        return false;
    }
    if(r->reason ==  TUNNEL_CONNECT_REASON_FORWARD) {
        debugprintf("We shall forward to %s:%d", r->host, r->port);

        //try to connect to remote destination
        m_mx->connect_port(r->host, r->port, [this, channel] (int port_socket) {
            debugprintf("Remote connection open");

            int result = m_mplex->add_endpoint_listener(channel, [this, port_socket](mplex * mpx, mplex_frame * frame) {
                //Copy everything we get from channel to socket
                if(frame==nullptr) {
                    debugprintf("nullptr on endpoint");
                    m_mx->remove_socket_callback(port_socket);
                    return false;
                }

                debugprintf( "EP: Received something on channel %d for socket %d", frame->channel, port_socket);
                int n=m_mx->awrite(port_socket, frame->payload.raw, frame->payload_size);
                if( n != frame->payload_size ) {
                    debugprintf("Error on socket write");
                    m_mx->remove_socket_callback(port_socket);
                    return false;
                } else {
                    return true;
                }

            });
            if(result < 0) {
                debugprintf("Error on connectiing port");
                return false;
            } else {
                m_mplex->add_endpoint_choke(channel, [this, port_socket](mplex * mpx, uint32_t channel, bool enabled) {
                    m_mx->choke(port_socket, enabled);
                });
            }

            result = m_mx->register_socket_callback(port_socket, [this, channel](int socket) {
                debugprintf("EP: Received something on socket %d for channel %d", socket, channel);
                //Copy everythig we receive from socket to channel
                mplex_frame frame;
                errno = 0;
                frame.payload_size = read(socket, frame.payload.raw, sizeof(frame.payload));
                if((frame.payload_size < 0) || ((frame.payload_size == 0) && (errno != EINPROGRESS))) {
                    debugprintf("error on read");
                    m_mplex->remove_endpoint_listener(channel);
                    return false;
                }
                if(frame.payload_size == 0) {
                    debugprintf("read 0, errno %s", strerror(errno));
                    //do not send answer in that case. Not EOF
                    return true;
                }
                m_mplex->send_data_response(channel, &frame);
                return true;
            });
            if(result < 0) {
                debugprintf("error on regitering socket callback");
                m_mplex->remove_endpoint_listener(channel);
                return false;
            } else {
                m_mx->add_socket_choke(port_socket, [this, channel](int socket, bool enabled) {
                    m_mplex->send_choke_response(channel, enabled);
                });
            }
            return true;
        });
    } else if(r->reason == TUNNEL_CONNECT_REASON_MCAST_FORWARD) {
        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(struct sockaddr_in));
        addr.sin_family=AF_INET;
        addr.sin_port=htons(r->port);
        addr.sin_addr.s_addr=inet_addr(r->host);
        debugprintf("We shall MCAST forward to %s:%d\n", r->host, r->port);

        //try to connect to remote destination
        int result = m_mx->add_udp_mcast_listener(r->host, r->port, [this, channel, addr] (int port_socket) {
            debugprintf("Remote mcast connection open");

            int result = m_mplex->add_endpoint_listener(channel, [this, port_socket, addr](mplex * mpx, mplex_frame * frame) {
                ssize_t addr_len = sizeof(addr);

                //Copy everything we get from channel to socket
                if(frame==nullptr) {
                    m_mx->remove_socket_callback(port_socket);
                    return false;
                }

                debugprintf("EP: Received %d something on mcast channel %d for %d", frame->payload_size, frame->channel,
                            port_socket);
                int n=sendto(port_socket, frame->payload.raw, frame->payload_size, 0, (struct sockaddr *)&addr, addr_len);
                if( n != frame->payload_size ) {
                    m_mx->remove_socket_callback(port_socket);
                    return false;
                } else {
                    return true;
                }

            });
            if(result < 0)
                return false;

            result = m_mx->register_socket_callback(port_socket, [this, channel](int port_socket) {
                struct sockaddr_in addr;
                socklen_t addr_len = sizeof(addr);
                debugprintf("EP: Received something on mcast socket %d for channel %d", port_socket, channel);
                //Copy everythig we receive from socket to channel
                mplex_frame frame;
                frame.payload_size = recvfrom(port_socket, frame.payload.raw, sizeof(frame.payload), 0, (struct sockaddr *)&addr,
                                              &addr_len);
                if(frame.payload_size <= 0) {
                    m_mplex->remove_endpoint_listener(channel);
                    return false;
                }
                m_mplex->send_data_response(channel, &frame);
                return true;
            });
            if(result < 0) {
                m_mplex->remove_endpoint_listener(channel);
                return false;
            };
            return true;
        });
        if(result < 0) {
            return false;
        }
    } else {
        debugprintf("Unknown reson %d", r->reason);
        return false;
    }
    return true;
}

int tunnel::open_udp_mcast(const char * target, uint16_t port, std::function<bool(mplex * mpx, uint32_t channel)> f) {
    tunnel_connect_reason reason;
    reason.reason = TUNNEL_CONNECT_REASON_MCAST_FORWARD;
    reason.port = port;
    strncpy(reason.host, target, sizeof(reason.host) -1);
    if(strlen (target) > sizeof(reason.host) -1) {
        debugprintf("WARNING: target URL too long");
    }
    return m_mplex->open_channel(f, (void*) &reason, sizeof(reason));
}

int tunnel::open_remote(const char * target, uint16_t port, std::function<bool(mplex * mpx, uint32_t channel)> f) {
    tunnel_connect_reason reason;
    reason.reason = TUNNEL_CONNECT_REASON_FORWARD;
    reason.port = port;
    strncpy(reason.host, target, sizeof(reason.host) -1);
    if(strlen (target) > sizeof(reason.host) -1) {
        debugprintf("WARNING: target URL too long");
    }
    return m_mplex->open_channel(f, (void*) &reason, sizeof(reason));
}

int tunnel::forward_port(const char* l_ip,uint16_t local_port, const char * target, uint16_t port,
                         std::function<void(tunnel* tn, int socket, uint32_t channel, std::shared_ptr<tunnel_filter>& send_filter, std::shared_ptr<tunnel_filter>& receive_filter)>
                         f) {

    return m_mx->add_port_listener(local_port, [this, l_ip, local_port, target, port, f](int newsocket) {
        debugprintf("New connection on %d", local_port);
        open_remote(target, port, [this, l_ip, target, port, f, newsocket, local_port](mplex * mpx, uint32_t channel) {
            if(channel <= 0) {
                //Remote rejected channel
                close(newsocket);
                return false;
            }
            debugprintf("Connected to %s:%d on channel %d", target, port, channel);
            std::string local_ip{l_ip};
            std::shared_ptr<tunnel_filter> send_filter = std::make_shared<tunnel_filter>();
            std::shared_ptr<tunnel_filter> receive_filter = std::make_shared<tunnel_filter>();
            f(this, newsocket, channel, send_filter, receive_filter);
            int result = m_mplex->add_channel_listener(channel, [this, newsocket, target, port, send_filter,
                  receive_filter](mplex * mpx, mplex_frame * frame) {
                //Copy everything we get from channel to socket
                if(frame==nullptr) {
                    debugprintf("nullptr from channel listener");
                    m_mx->remove_socket_callback(newsocket);
                    return false;
                }

                debugprintf("SO: Received something on channel %d", frame->channel);
                if(!receive_filter->process((const char*)frame->payload.raw, frame->payload_size, [this, newsocket, send_filter,
                                                  receive_filter](const char * data,
                const size_t data_length) {
                if(data_length == 0)
                        return true;

                    int n=m_mx->awrite(newsocket, data, data_length);
                    if( n != data_length ) {
                        m_mx->remove_socket_callback(newsocket);
                        return false;
                    }
                    return true;
                })) {
                    m_mx->remove_socket_callback(newsocket);
                    return false;
                }
                return true;
            });
            if(result <0) {
                return false;
            } else {
                m_mplex->add_channel_choke(channel, [this, newsocket](mplex * mpx, uint32_t channel, bool enabled) {
                    m_mx->choke(newsocket, enabled);
                });
            }

            result = m_mx->register_socket_callback(newsocket, [this, channel, send_filter, receive_filter](int readsocket) {
                debugprintf("SO: Received something on socket for channel %d", channel);
                //Copy everythig we receive from socket to channel
                mplex_frame frame;
                errno = 0;
                frame.payload_size = read(readsocket, frame.payload.raw, sizeof(frame.payload));
                debugprintf("n==%d %s", frame.payload_size, strerror(errno));
                if(frame.payload_size < 0) {
                    m_mplex->remove_channel_listener(channel);
                    return false;
                }
                if((frame.payload_size == 0) && (errno == EINPROGRESS)) {
                    //Short read, but not EOF. Continue
                    return true;
                }
                if(!send_filter->process((const char*)frame.payload.raw, frame.payload_size, [this, channel,
                                               send_filter](const char * data,
                const size_t data_length) {
                size_t length = data_length;
                mplex_frame frame;
                while(length > 0) {
                        if(length > sizeof(frame.payload)) {
                            memcpy(frame.payload.raw, data, sizeof(frame.payload));
                            frame.payload_size = sizeof(frame.payload);
                            length -= sizeof(frame.payload);
                            data += sizeof(frame.payload);
                        } else {
                            memcpy(frame.payload.raw, data, length);
                            frame.payload_size = length;
                            length = 0;
                        }
                        if(m_mplex->send_data(channel, &frame) < 0) {
                            return false;
                        }
                    }
                    return true;
                })) {
                    m_mplex->remove_channel_listener(channel);
                    return false;
                }
                return true;
            });
            if(result < 0) {
                m_mplex->remove_channel_listener(channel);
                return false;
            } else {
                m_mx->add_socket_choke(newsocket, [this, channel](int socket, bool enabled) {
                    m_mplex->send_choke(channel, enabled);
                });
            }
            return true;
        });
    });
}

void tunnel::rewoke_forward(uint16_t local_port) {
    m_mx->remove_port_listener(local_port);
    free_local_port(local_port);
}

bool tunnel::receive(int socket) {
    if(m_mplex != nullptr)
        return m_mplex->receive(socket);
    else
        errorprintf("Loosing mplex message!");
    return false;
}
static uint16_t pp{50000};
uint16_t tunnel::get_local_port() {
    return pp++;
}

void tunnel::free_local_port(uint16_t port) {
}

