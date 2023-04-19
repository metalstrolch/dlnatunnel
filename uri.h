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

//Stolen from: https://stackoverflow.com/questions/2616011/easy-way-to-parse-a-url-in-c-cross-platform

#include <string>
#include <algorithm>    // find

struct Uri {
public:
    std::string QueryString, Path, Protocol, Host, Port;

    static Uri Parse(const std::string &uri);
};

