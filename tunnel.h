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

#ifndef __TUNNEL_H
#define __TUNNEL_H
#include <functional>
#include <memory>
#include "mplex.h"
#include "socketmultiplex.h"
#include "tunnel_filter.h"

class tunnel {
public:
    tunnel(socketmultiplex * mx, int socket, std::function<void(tunnel* tn)> on_ready);
    ~tunnel();
    void run();

    int open_remote(const char * target, uint16_t port, std::function<bool(mplex * mpx, uint32_t channel)> f);
    int open_udp_mcast(const char * target, uint16_t port, std::function<bool(mplex * mpx, uint32_t channel)> f);
    int forward_port(const char * local_ip, uint16_t local_port, const char * target, uint16_t port,
                     std::function<void(tunnel* tn, int socket, uint32_t channel, std::shared_ptr<tunnel_filter>& send_filter, std::shared_ptr<tunnel_filter>& receive_filter)>
                     f);
    void rewoke_forward(uint16_t local_port);

    bool receive(int socket);
    static uint16_t get_local_port();
    static void free_local_port(uint16_t port);
private:
    socketmultiplex * m_mx;
    mplex *m_mplex;
    int m_socket;
    std::function<void(tunnel*tn)> m_on_ready;

    void on_mplex_ready(mplex* mpx);
    bool on_mplex_connect(mplex * mpx, uint32_t channel, void* reason, uint8_t size);
};

#endif
