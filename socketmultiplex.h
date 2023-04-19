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

#ifndef __LIBSOCKETMULTIPLEX_H
#define __LIBSOCKETMULTIPLEX_H
#include <stdint.h>
#include <vector>
#include <functional>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <vector>

struct listener_helper {
    std::function<void(int socket)> f{};
    int socket{0};
    uint16_t listen_port;
};

struct socket_helper {
    std::function<bool(int socket)> f{};
    std::function<void(int socket, bool enabled)> onChoke{};
    int socket{0};
    std::vector<uint8_t> writebuffer{};
    bool choked{false};
    bool choke_requested{false};
};

class socketmultiplex {
public:
    socketmultiplex();
    ~socketmultiplex();
    int add_udp_mcast_listener(const char * url, uint16_t port, std::function<bool(int socket)> f);
    int connect_port(const char * url, uint16_t port, std::function<bool(int socket)> f);

    int add_port_listener(uint16_t listen_port, std::function<void(int socket)> f);
    void remove_port_listener(uint16_t listen_port);
    int register_socket_callback(int socket, std::function<bool(int socket)> f);
    void remove_socket_callback(int socket);
    void add_socket_choke(uint16_t socket, std::function<void(int socket, bool enabled)> onChoke);

    ssize_t awrite(int socket, const void *buf, size_t count, bool block=false);
    void choke(int socket, bool enable);

    void handle_sockets(struct timeval tv);
private:
    void remove_attempt(int socket);
    std::vector<listener_helper> listener;
    std::vector<socket_helper> connections;
    std::vector<socket_helper> attempts;
    bool m_processing_listener{false};
    bool m_processing_connections{false};
    bool m_processing_attempts{false};
};

#endif
