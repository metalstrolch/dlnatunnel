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
#include <string>
#include "tunnel_filter.h"

bool tunnel_filter::process(const char * data, const size_t data_length,
                            std::function<bool(const char * data, const size_t data_length)> f) {
    if(data_length == 0) {
        f(data, data_length);
        return false;
    } else {
        return f(data, data_length);
    }
}

