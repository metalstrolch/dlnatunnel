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

#ifndef __DLNA_FILTER_H
#define __DLNA_FILTER_H

#include <functional>
#include <string>

#include "http.h"
#include "collector.h"


class dlna_filter : public http {
public:
    dlna_filter(dlna_host * host, std::function<void(uint16_t port)> f);
    virtual ~dlna_filter();

private:
    std::string filter_http(const std::string & http_data) override;
    dlna_host* m_host;
    std::function<void(uint16_t port)> m_f;
};



#endif
