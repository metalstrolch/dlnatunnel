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

#include <string>
#include <vector>
#include <map>

#include "collector.h"
#include "uri.h"
#include "tunnel.h"
#include "dlna_filter.h"

#include "debugprintf.h"

static std::string make_key(std::string host, std::string port) {
    return host + std::string(":") + port;
}

static std::string make_key(std::string host, uint16_t port) {
    return host + std::string(":") + std::to_string(port);
}

collector::collector(std::string local_ip):
    m_local_ip{local_ip} {
    debugprintf("Using local IP %s", m_local_ip.data());
};

collector::~collector()
{};

void collector::use_tunnel(tunnel* tn) {
    m_tn = tn;
    open_ssdp();
}

void collector::use_px(socketmultiplex* local_px) {
    m_local_px = local_px;
    open_local_ssdp();
}

void collector::handle_answer_message(ssdp_peer * peer) {
    Uri loc = Uri::Parse(peer->LOCATION.data());
    std::string key = make_key(loc.Host, loc.Port);
    //Check if already in map
    auto dlnahost = hosts.find(key);
    if(dlnahost == hosts.end()) {
        debugprintf("Found new host %s", key.data());
        dlna_host host;
        host.host = loc.Host;
        host.port = atoi(loc.Port.data());
        host.messages.push_back({*peer});
        hosts.insert({key, host});
        handle_new_host(key);
    } else {
        for(auto message : dlnahost->second.messages) {
            if(message.peer == *peer) {
                debugprintf("Drop message. Already there.");
                return;
            }
        }
        dlnahost->second.messages.push_back({*peer});
    }
    dlnahost = hosts.find(key);
    if(dlnahost != hosts.end()) {
        std::string forward = m_ssdp.createNotify(std::string("ssdp:alive"),peer,dlnahost->second.tunnel_host,
                              dlnahost->second.tunnel_port );
        debugprintf("Forward alive : %s", forward.data());
        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(struct sockaddr_in));
        addr.sin_family=AF_INET;
        addr.sin_port=htons(1900);
        addr.sin_addr.s_addr=inet_addr("239.255.255.250");
        int m =sendto(m_local_ssdp, forward.data(), forward.size(), 0, (struct sockaddr *)&addr, sizeof(struct sockaddr_in));
    }
};

void collector::handle_notify_message(ssdp_peer * peer) {
    //What does the peer want us to say?
    if(peer->NTS.compare(std::string("ssdp:alive")) == 0) {
        debugprintf("Alive");
        handle_answer_message(peer);
        //Alive message
    } else if(peer->NTS.compare(std::string("ssdp:byebye")) == 0) {
        debugprintf("BYEBYE");
        //ByeBye messages don't tell location. so we need to find the service per message.
        for(auto& host: hosts) {
            if(!host.second.messages.empty()) {
                host.second.messages.erase(std::remove_if(host.second.messages.begin(),
                host.second.messages.end(), [this, host, peer] (dlna_message& message) {
                    if(message.peer == *peer) {
                        debugprintf("Byebye message.");
                        std::string forward = m_ssdp.createNotify(std::string("ssdp:byebye"),peer, host.second.tunnel_host,
                                              host.second.tunnel_port);
                        debugprintf("Forward byebye: %s", forward.data());
                        struct sockaddr_in addr;
                        memset(&addr, 0, sizeof(struct sockaddr_in));
                        addr.sin_family=AF_INET;
                        addr.sin_port=htons(1900);
                        addr.sin_addr.s_addr=inet_addr("239.255.255.250");
                        int m =sendto(m_local_ssdp, forward.data(), forward.size(), 0, (struct sockaddr *)&addr, sizeof(struct sockaddr_in));
                        return true;
                    }
                    return false;
                }), host.second.messages.end());
                //remove if no messages left.
                if(host.second.messages.empty()) {
                    debugprintf("Byebye host: %s", host.first.data());
                    handle_lose_host(host.first);
                    hosts.erase(host.first);
                    //iterator destroyed! abort
                    break;
                }

            }

        }
    } else {
        errorprintf("Unhandled notify type %s", peer->NTS.data());
    }
}

void collector::add_message(char * message, uint32_t message_size) {
    ssdp_peer peer;
    switch(m_ssdp.parse_message(message, message_size, &peer)) {
    case    SSDP_TYPE_UNKNOWN:
        debugprintf("Drop UNKOWN SSDP message");
        break;
    case     SSDP_TYPE_SEARCH:
        debugprintf("Drop SEARCH SSDP message");
        break;
    case     SSDP_TYPE_ANSWER:
        debugprintf("ANSWER SSDP message");
        handle_answer_message(&peer);
        break;
    case    SSDP_TYPE_NOTIFY:
        debugprintf("NOTIFY SSDP message %s", peer.raw.data());
        handle_notify_message(&peer);
        break;
    }
    for(auto h: hosts) {
        debugprintf("%s:%d:%d -> %ld",h.second.host.data(), h.second.port, h.second.tunnel_port, h.second.messages.size());
    }
}

void collector::handle_local_search_message(ssdp_peer * peer, struct sockaddr_in * addr) {
    debugprintf("Search for %s", peer->ST.data());
    for(auto h : hosts) {
        for(auto peer: h.second.messages) {
            std::string message;
            message = m_ssdp.createAnswer(&(peer.peer),h.second.tunnel_host, h.second.tunnel_port);
            debugprintf("-->> %s", message.data());
            if(m_local_ssdp != 0) {
                int m =sendto(m_local_ssdp, message.data(), message.size(), 0, (struct sockaddr *)addr, sizeof(struct sockaddr_in));
                debugprintf("n==%d, size=%ld",m, message.size());
            }
        }
    }
}

void collector::add_local_message(char * message, uint32_t message_size, struct sockaddr_in * addr) {
    ssdp_peer peer;
    switch(m_ssdp.parse_message(message, message_size, &peer)) {
    case    SSDP_TYPE_UNKNOWN:
        debugprintf("Drop UNKOWN local SSDP message");
        break;
    case     SSDP_TYPE_SEARCH:
        debugprintf("local SEARCH SSDP message");
        handle_local_search_message(&peer, addr);
        break;
    case     SSDP_TYPE_ANSWER:
        debugprintf("Drop ANSWER SSDP local message");
        break;
    case    SSDP_TYPE_NOTIFY:
        debugprintf("Drop NOTIFY SSDP local message");
        break;
    }
    for(auto h: hosts) {
        debugprintf("%s:%d:%d -> %ld",h.second.host.data(), h.second.port, h.second.tunnel_port, h.second.messages.size());
    }
}

void collector::open_ssdp () {
    if(m_tn == nullptr) {
        errorprintf("Need to call use_tunnel() first wit working tunnel");
        return;
    }
    m_tn->open_udp_mcast("239.255.255.250", 1900, [this](mplex * mpx, uint32_t channel) {
        char data[] =
            "M-SEARCH * HTTP/1.1\r\n"
            "HOST: 239.255.255.250:1900\r\n"
            "MAN: \"ssdp:discover\"\r\n"
            "ST: ssdp:all\r\n"
            "MX: 120\r\n"
            "USER-AGENT: Jolla/1.0 UPnP/1.1\r\n"
            "\r\n";
        mplex_frame frame;
        frame.payload_size=sprintf((char *)frame.payload.raw, "%s", data);
        mpx->send_data(channel, &frame);

        mpx->add_channel_listener(channel,[this] (mplex * mpx, mplex_frame * frame) {
            if(frame == nullptr)
                return false;
            frame->payload.raw[frame->payload_size -1] = 0;
            add_message((char*)frame->payload.raw, frame->payload_size);
            return true;
        });
        return true;
    });
}

void collector::open_local_ssdp () {
    m_local_px->add_udp_mcast_listener("239.255.255.250", 1900, [this](int socket) {
        if(socket <= 0) {
            m_local_ssdp=0;
            return false;
        }
        debugprintf("Local mcast socket open");
        m_local_ssdp = socket;
        int result = m_local_px->register_socket_callback(socket, [this](int socket) {
            struct sockaddr_in addr;
            memset(&addr, 0, sizeof(struct sockaddr_in));
            socklen_t addr_len = sizeof(struct sockaddr_in);
            mplex_frame frame;
            frame.payload_size = recvfrom(socket, frame.payload.raw, sizeof(frame.payload), 0, (struct sockaddr *)&addr,
                                          &addr_len);
            if(frame.payload_size <= 0) {
                m_local_ssdp=0;
                return false;
            }
            frame.payload.raw[frame.payload_size -1] = 0;
            add_local_message((char*)frame.payload.raw, frame.payload_size, &addr);
            return true;
        });
        if(result <= 0) {
            return false;
        }
        return true;
    });
}

void collector::handle_new_host(std::string key) {
    auto host = hosts.find(key);
    if(host == hosts.end()) {
        errorprintf("Unable to find new host %s", key.data());
        return;
    }
    host->second.tunnel_host = m_local_ip;
    host->second.tunnel_port = m_tn->get_local_port();
    //Remember the control port, so we don't start the tunnel a second time
    host->second.ports.insert({host->second.port, host->second.tunnel_port});
    debugprintf("port forwarding for %s on %d", key.data(), host->second.tunnel_port);
    m_tn->forward_port(host->second.tunnel_host.data(), host->second.tunnel_port, host->second.host.data(),
                       host->second.port,
                       [this, key](tunnel* tn, int socket, uint32_t channel, std::shared_ptr<tunnel_filter>& send_filter,
    std::shared_ptr<tunnel_filter>& receive_filter) {
        auto host = hosts.find(key);
        if(host == hosts.end()) {
            errorprintf("Unable to find new host %s", key.data());
            return;
        }
        std::function<void(uint16_t)> on_additional_port = [this, key](uint16_t port) {
            auto host = hosts.find(key);
            if(host == hosts.end()) {
                errorprintf("Unable to find new host %s", key.data());
                return;
            }
            auto port_forward = host->second.ports.find(port);
            if(port_forward == host->second.ports.end()) {
                auto& h = host->second;
                uint16_t tunnel_port=m_tn->get_local_port();
                h.ports.insert({port, tunnel_port});
                debugprintf("opening tunnel: %s:%d->%s:%d", h.tunnel_host.data(), tunnel_port, h.host.data(), port);
                m_tn->forward_port(h.tunnel_host.data(), tunnel_port, h.host.data(), port,
                                   [this, key](tunnel* tn, int socket, uint32_t channel, std::shared_ptr<tunnel_filter>& send_filter,
                std::shared_ptr<tunnel_filter>& receive_filter) {
                    auto host = hosts.find(key);
                    if(host == hosts.end()) {
                        errorprintf("Unable to find host %s", key.data());
                        return;
                    }
                    auto& h = host->second;
                    debugprintf("opened tunnel: %s:%d->%s:%d", h.host.data(), h.port, h.tunnel_host.data(), h.tunnel_port);
                    //Here we could install channel filters of the addiional data channels. But we don't. This
                    //does not seem to be neccesary as only data is transferred here.
                });
            }
        };
        send_filter.reset(new dlna_filter(&(host->second),on_additional_port));
        receive_filter.reset(new dlna_filter(&(host->second),on_additional_port));
        debugprintf("port forwarding established for %s on %d", key.data(), host->second.tunnel_port);
    });
}

void collector::handle_lose_host(std::string key) {
    auto host = hosts.find(key);
    if(host == hosts.end()) {
        errorprintf("Unable to find died host %s", key.data());
        return;
    }
    for(auto port: host->second.ports) {
        m_tn->rewoke_forward(port.first);
    }
}
