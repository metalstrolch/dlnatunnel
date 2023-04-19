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

#ifndef __COLLECTOR_H
#define __COLLECTOR_H
#include <string>
#include <vector>
#include <map>
#include "ssdp.h"
#include "tunnel.h"

struct dlna_message {
    //timestanp
    ssdp_peer peer;
};

struct dlna_host {
    std::string host{};
    uint16_t port{0};
    std::string tunnel_host{};
    uint16_t tunnel_port{0};
    std::vector<dlna_message> messages{};
    std::map<uint16_t, uint16_t> ports{};
};

class collector {
public:
    collector(std::string local_ip);
    ~collector();

    void add_message(char * message, uint32_t message_size);
    void add_local_message(char * message, uint32_t message_size, struct sockaddr_in * addr);
    void use_tunnel(tunnel* tn);
    void use_px(socketmultiplex* local_px);

private:
    void handle_local_search_message(ssdp_peer * peer, struct sockaddr_in *addr);
    void handle_answer_message(ssdp_peer * peer);
    void handle_notify_message(ssdp_peer * peer);
    void handle_new_host(std::string key);
    void handle_lose_host(std::string key);
    void open_ssdp ();
    void open_local_ssdp ();
    std::map<std::string, dlna_host> hosts{};
    ssdp m_ssdp{};
    tunnel * m_tn{nullptr};
    socketmultiplex * m_local_px;
    int m_local_ssdp{0};
    std::string m_local_ip{};

};

#endif

