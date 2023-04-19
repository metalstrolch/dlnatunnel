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

#include <stdio.h>
#include <malloc.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string>
#include <regex>

#include "stringtoken.h"
#include "debugprintf.h"

int read_line( char** buffer, int* size, const char* input, size_t input_size, bool* have_newline) {
    if(input == nullptr) {
        *buffer = nullptr;
        return 0;
    }
    int buffersize = 255;
    size_t pos=0;
    *size = 0;
    *buffer = (char *)  malloc( buffersize );
    (*buffer)[0] = 0;
    if(have_newline != nullptr)
        *have_newline=false;

    while (pos < input_size) {
        char c = input[pos];
        pos ++;

        if(c == '\n') {
            if(have_newline != nullptr)
                *have_newline = true;
            break;
        } else if(c == 0) {
            break;
        } else if(c == '\r') {
            continue;
        } else {
            (*buffer)[*size] = c;
            (*buffer)[(*size)+1] = 0; //always close buffer
            (*size) ++;
            if( ( *size ) > ( buffersize - 2 ) ) {
                buffersize += 200;
                *buffer = (char *) realloc( ( *buffer ), buffersize );
            }
        }

    }
    if(pos == 0) {
        free(*buffer);
        *buffer = nullptr;
    }
    return pos;
}

int read_token( char* buffer, int* size, const char* string, const int stringsize ) {
    int max = *size;
    ( *size ) = 0;
    buffer[0] = 0;
    int a = 0;

    //Skip leading spaces;
    while( ( ( string[a] == ' ' ) || ( string[a] == '\t' ) ) && ( a < stringsize ) ) {
        a++;
    }

    while( ( ( string[a] != ' ' ) && ( string[a] != '\t' ) && (string[a] != ':'))  && ( a < stringsize )
            && ( ( *size ) < max ) ) {
        buffer[( *size )] = string[a];
        buffer[( *size ) + 1] = 0;
        a++;
        ( *size ) ++;
    }

    //Skip trailing spaces for next token;
    while( ( ( string[a] == ' ' ) || ( string[a] == '\t' ) || (string[a] == ':') ) && ( a < stringsize ) ) {
        a++;
    }

    return a;
}

bool parse_token(const char * find_token, const char * line, int line_length, std::string * string) {
    const char * pos = line;
    int left = line_length;
    int n;
    char token[255];
    int tokensize=sizeof(token);
    n = read_token(token,  &tokensize, pos, left);
    if(tokensize > 0) {
        for(int a = 0; a < tokensize; a++)
            token[a] = std::toupper(token[a]);
        if(strcmp(token, find_token) == 0) {
            pos += n;
            left -= n;
            (*string)=std::string(pos);
            return true;
        }
    }
    return false;
}

std::string do_replace_case( std::string const & in, std::string const & from, std::string const & to ) {
    return std::regex_replace( in.data(), std::regex(from, std::regex_constants::icase), to.data());
}

bool iequals(const std::string& a, const std::string& b) {
    return std::equal(a.begin(), a.end(),
                      b.begin(), b.end(),
    [](char a, char b) {
        return std::tolower(a) == std::tolower(b);
    });
}

bool icontains(const std::string& a, const std::string& b) {
    std::string low_a;
    std::string low_b;
    for(char x: a)
        low_a += std::tolower(x);

    for(char x: b)
        low_b += std::tolower(x);
    if (low_a.find(low_b) != std::string::npos) {
        return true;
    }
    return false;
}
