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

#ifndef __SSDP_H
#define __SSDP_H
#include <vector>
#include <string>

enum ssdp_type {
    SSDP_TYPE_UNKNOWN,
    SSDP_TYPE_SEARCH,
    SSDP_TYPE_ANSWER,
    SSDP_TYPE_NOTIFY,
};

struct ssdp_peer {
//	NOTIFY * HTTP/1.1
    ssdp_type type{SSDP_TYPE_UNKNOWN};
    std::string SERVER{};
    std::string CACHE_CONTROL{};
    std::string LOCATION{};
    std::string NTS{};
    std::string NT{};
    std::string USN{};
    std::string HOST{};
    std::string ST{};
    std::string DATE{};
    std::string EXT{};
    std::string MAN{};
    std::string MX{};
    std::string Content_Length{};
    std::string USER_AGENT{};

    std::string unknown{};
    std::string raw{};
    bool operator == (const ssdp_peer &other);
};

class ssdp {
public:
    ssdp();
    ~ssdp();

    ssdp_type parse_message(const char *mesage, int message_size, ssdp_peer * peer);
    std::string createAnswer(ssdp_peer *peer, std::string host, uint16_t port);
    std::string createNotify(std::string type, ssdp_peer *peer, std::string host, uint16_t port);
private:

    std::vector<ssdp_peer> m_peers;
};
#endif
