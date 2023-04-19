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

#include <vector>
#include <string>
#include "ssdp.h"
#include "stringtoken.h"
#include "string.h"
#include "uri.h"
#include "debugprintf.h"

#define SSDP_NL std::string("\r\n")

bool ssdp_peer::operator == (const ssdp_peer &other) {
    if(type == other.type) {
//        debugprintf("Equal type");
        if(ST.compare(other.ST) == 0) {
            if(USN.compare(other.USN) == 0) {
                debugprintf("OK");
                return true;
            }
        }

    } else if((type == SSDP_TYPE_NOTIFY) && (other.type == SSDP_TYPE_ANSWER)) {
//        debugprintf("NOTIFY == ANSWER");
        if(NT.compare(other.ST) == 0) {
            if(USN.compare(other.USN) == 0) {
                debugprintf("OK");
                return true;
            }
        }

    } else if((other.type == SSDP_TYPE_NOTIFY) && (type == SSDP_TYPE_ANSWER)) {
//        debugprintf("ANSWER == NOTIFY");
        if(ST.compare(other.NT) == 0) {
            if(USN.compare(other.USN) == 0) {
                debugprintf("OK");
                return true;
            }
        }

    } else if((type == SSDP_TYPE_ANSWER) && (other.type == SSDP_TYPE_SEARCH)) {
//        debugprintf("NOTIFY == ANSWER");
        if(NT.compare(other.ST) == 0) {
            debugprintf("OK");
            return true;
        }

    } else if((other.type == SSDP_TYPE_ANSWER) && (type == SSDP_TYPE_SEARCH)) {
//        debugprintf("NOTIFY == ANSWER");
        if(NT.compare(other.ST) == 0) {
            debugprintf("OK");
            return true;
        }

    } else {
        return false;
    }

    return false;
}

ssdp::ssdp() : m_peers{} {
}

ssdp::~ssdp() {
}

ssdp_type ssdp::parse_message(const char *message, int message_size,  ssdp_peer * peer) {
    char * line = nullptr;
    int line_length = 0;
    int pos=0;
    int n;
    std::string temp;
    while((n=read_line( &line, &line_length, message + pos, message_size - pos)) > 0) {
        if(line_length == 0) {
            debugprintf("Got empty line");

        } else if(parse_token("HTTP/1.1", line, line_length, &temp)) {
            peer->type=SSDP_TYPE_ANSWER;
        } else if(parse_token("HTTP/1.0", line, line_length, &temp)) {
            peer->type=SSDP_TYPE_ANSWER;
        } else if(parse_token("NOTIFY", line, line_length, &temp)) {
            peer->type=SSDP_TYPE_NOTIFY;
        } else if(parse_token("M-SEARCH", line, line_length, &temp)) {
            peer->type=SSDP_TYPE_SEARCH;
        } else if(!parse_token("SERVER", line, line_length, &(peer->SERVER)))
            if(!parse_token("CACHE-CONTROL", line, line_length, &(peer->CACHE_CONTROL)))
                if(!parse_token("LOCATION", line, line_length, &(peer->LOCATION)))
                    if(!parse_token("HOST", line, line_length, &(peer->HOST)))
                        if(!parse_token("NT", line, line_length, &(peer->NT)))
                            if(!parse_token("NTS", line, line_length, &(peer->NTS)))
                                if(!parse_token("USN", line, line_length, &(peer->USN)))
                                    if(!parse_token("ST", line, line_length, &(peer->ST)))
                                        if(!parse_token("DATE", line, line_length, &(peer->DATE)))
                                            if(!parse_token("EXT", line, line_length, &(peer->EXT)))
                                                if(!parse_token("MAN", line, line_length, &(peer->MAN)))
                                                    if(!parse_token("MX", line, line_length, &(peer->MX)))
                                                        if(!parse_token("CONTENT-LENGTH", line, line_length, &(peer->Content_Length)))
                                                            if(!parse_token("USER-AGENT", line, line_length, &(peer->USER_AGENT))) {
                                                                debugprintf("UNABLE To PARSE %s", line);
                                                                peer->unknown += (std::string(line) + SSDP_NL);
                                                            }

        free(line);
        pos +=n;
    }
    peer->raw = std::string(message);
    return peer->type;
}


std::string ssdp::createAnswer(ssdp_peer *peer, std::string host, uint16_t port) {
    Uri target = Uri::Parse(peer->LOCATION);
    std::string result{};
    result += (std::string("HTTP/1.1 200 OK") + SSDP_NL);
    result += (std::string("LOCATION: http://") + host + std::string(":") + std::to_string(port) + target.Path + SSDP_NL);
    result += (std::string("SERVER: ") + peer->SERVER +SSDP_NL);
    result += (std::string("CACHE-CONTROL: ") + peer->CACHE_CONTROL +SSDP_NL);
    result += (std::string("EXT: ") + peer->EXT +SSDP_NL);
    result += (std::string("ST: ") + peer->ST +SSDP_NL);
    result += (std::string("USN: ") + peer->USN +SSDP_NL);
    result += (SSDP_NL);


    return result;
}

std::string ssdp::createNotify(std::string type, ssdp_peer *peer, std::string host, uint16_t port) {
    Uri target = Uri::Parse(peer->LOCATION);
    std::string result{};
    result += (std::string("NOTIFY * HTTP/1.1") + SSDP_NL);
    result += (std::string("HOST: 239.255.255.250:1900") + SSDP_NL);
    result += (std::string("LOCATION: http://") + host + std::string(":") + std::to_string(port) + target.Path + SSDP_NL);
    result += (std::string("SERVER: ") + peer->SERVER +SSDP_NL);
    result += (std::string("CACHE-CONTROL: ") + peer->CACHE_CONTROL +SSDP_NL);
    result += (std::string("NT: ") + peer->NT +SSDP_NL);
    result += (std::string("NTS: ") + type +SSDP_NL);
    result += (std::string("USN: ") + peer->USN +SSDP_NL);
    result += (SSDP_NL);

    return result;
}

