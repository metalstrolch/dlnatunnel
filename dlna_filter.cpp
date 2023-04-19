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


#include <functional>
#include <regex>
#include <string>

#include "stringtoken.h"
#include "dlna_filter.h"
#include "collector.h"
#include "debugprintf.h"

static void find_ports(const std::string& a, const std::string& b, std::function<void(uint16_t port)> f) {
    std::regex re(b+std::string(":[0-9]+"), std::regex_constants::icase);
    std::sregex_iterator next(a.begin(), a.end(), re);
    std::sregex_iterator end;
    while (next != end) {
        std::smatch match = *next;
        size_t found = match.str().find_last_of(":");
        f(atoi(match.str().substr(found+1).data()));
        next++;
    }
}

dlna_filter::dlna_filter(dlna_host * host, std::function<void(uint16_t port)> f): http {host->host + std::string(":") + std::to_string(host->port)},
    m_host{host},
    m_f{f} {
}

dlna_filter::~dlna_filter() {
}

std::string dlna_filter::filter_http(const std::string & http_data) {
    find_ports(http_data, std::string(m_host->host), [this](uint16_t port) {
        m_f(port);
    });
    std::string temp = http_data;
    for(auto port: m_host->ports) {
        temp = do_replace_case(temp, m_host->host + std::string(":") + std::to_string(port.first),
                               m_host->tunnel_host + std::string(":") + std::to_string(port.second));
    }
    return temp;
}

