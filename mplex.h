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

#ifndef __MPLEX_H
#define __MPLEX_H
#include <stdint.h>
#include <functional>
#include "socketmultiplex.h"

class mplex;

#define MPLEX_TYPE_RESPONSE 0x0001
#define MPLEX_TYPE_DATA     0x0000
#define MPLEX_TYPE_HELLO    0x1000
#define MPLEX_TYPE_OPEN     0x2000
#define MPLEX_TYPE_CLOSE    0x3000
#define MPLEX_TYPE_CHOKE    0x4000

#pragma pack(push,1)
struct mplex_frame {
    uint8_t magic[4] {'M','P','L','X'};
    uint16_t type{MPLEX_TYPE_DATA};
    uint16_t channel{0};
    int32_t payload_size{0};
    union {
        uint8_t raw [1024*100] {};
        struct {
            bool failure;
            uint8_t reason_size;
            uint8_t reason[256];
        } open;
        struct {
            bool enable;
        } choke;
    } payload ;
} ;
#pragma pack(pop)

struct mplex_listener_helper {
    uint32_t channel{0};
    std::function<bool(mplex * mpx, uint32_t channel)> f{};
    std::function<void(mplex * mpx, uint32_t channel, bool enabled)> onChoke{};
};

struct mplex_channel_helper {
    uint32_t channel{0};
    std::function<bool(mplex * mpx, mplex_frame * frame)> f{};
    std::function<void(mplex * mpx, uint32_t channel, bool enabled)> onChoke{};
};

class mplex {

public:
    mplex(socketmultiplex* mx, int socket, std::function<void(mplex* mpx)> on_ready,
          std::function<bool(mplex * mpx, uint32_t channel, void* reason, uint8_t size)> on_connect);
    ~mplex();

    int open_channel(std::function<bool(mplex * mpx, uint32_t channel)> f, void * reason=nullptr, uint8_t size=0);
    int add_channel_listener(uint32_t channel, std::function<bool(mplex * mpx, mplex_frame * frame)> f);
    void add_channel_choke(uint32_t channel, std::function<void(mplex * mpx, uint32_t channel, bool enabled)> onChoke);
    void add_endpoint_choke(uint32_t channel, std::function<void(mplex * mpx, uint32_t channel, bool enabled)> onChoke);
    void remove_channel_listener(uint32_t channel);
    int add_endpoint_listener(uint32_t channel, std::function<bool(mplex * mpx, mplex_frame * frame)> f);
    void remove_endpoint_listener(uint32_t channel);

    int send_data(uint32_t channel, mplex_frame* frame);
    int send_data_response(uint32_t channel, mplex_frame* frame);

    void send_choke(uint32_t channel, bool enable);
    void send_choke_response(uint32_t channel, bool enable);

    bool receive(int socket);
private:
    void close_all();
    void remove_attempt(uint32_t channel);
    bool process_frame(mplex_frame* frame);
    void send_hello();
    void send_hello_response(mplex_frame* frame);
    void send_open(uint32_t channel, void* reason=nullptr, uint8_t size=0);
    void send_open_response(uint32_t channel, bool failure);
    void send_close(uint32_t channel);
    void send_close_response(uint32_t channel);
    mplex_frame m_buffer;
    uint32_t m_buffered;
    uint32_t m_free_channel;
    bool m_ready;
    int m_socket;
    socketmultiplex* m_mx;
    std::function<void(mplex* mpx)> m_on_ready;
    std::function<bool(mplex* mpx, uint32_t channel, void* reason, uint8_t size)> m_on_connect;

    std::vector<mplex_listener_helper> m_attempts;
    std::vector<mplex_channel_helper> m_channels;
    std::vector<mplex_channel_helper> m_endpoints;
};


#endif
