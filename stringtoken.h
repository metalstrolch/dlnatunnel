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

#ifndef STRINGTOKEN_H
#define STRINGTOKEN_H

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string>

int read_line( char** buffer, int* size, const char* input, size_t input_size, bool* have_newline=nullptr);

int read_token( char* buffer, int* size, const char* string, const int stringsize );

bool parse_token(const char * find_token, const char * line, int line_length, std::string * string);

std::string do_replace_case( std::string const & in, std::string const & from, std::string const & to );

bool iequals(const std::string& a, const std::string& b);
bool icontains(const std::string& a, const std::string& b);

#endif

