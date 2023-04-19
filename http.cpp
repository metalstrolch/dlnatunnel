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
#include "http.h"
#include "stringtoken.h"

#include "debugprintf.h"

#define HTTP_NL std::string("\r\n")

http::http( std::string realHost ):
    m_realHost{realHost} {
    debugprintf("created");
}

http::~http() {
    debugprintf("destroyed");
}

bool http::parse_header_line(const char * line, size_t line_length) {
    if(parse_token("HOST", line, line_length, &(m_header.HOST)))
        return true;
    if(parse_token("CONTENT-LENGTH", line, line_length, &(m_header.CONTENT_LENGTH)))
        return true;
    if(parse_token("CONTENT-TYPE", line, line_length, &(m_header.CONTENT_TYPE)))
        return true;
    if(parse_token("TRANSFER-ENCODING", line, line_length, &(m_header.TRANSFER_ENCODING)))
        return true;
    return false;
}

void http::complete_http_header() {
    m_header_data += (std::string("HOST: ") + m_realHost + HTTP_NL);
    if(!m_chunked)
        m_header_data += (std::string("CONTENT-LENGTH: ") + m_header.CONTENT_LENGTH + HTTP_NL);
    else
        m_header_data += (std::string("TRANSFER-ENCODING: chunked") + HTTP_NL);
    m_header_data += (std::string("CONTENT-TYPE: ") + m_header.CONTENT_TYPE + HTTP_NL);
    m_header_data += HTTP_NL;
}

bool http::flush_and_reset(std::function<bool(const char * data, const size_t data_length)> f) {
    debugprintf("Flush remaining");
    if(m_state == HTTP_STATE_HEADER) {
        m_header_data += m_rest;
        if(!f(m_header_data.data(), m_header_data.size())) {
            return false;
        }

    }
    if(m_state == HTTP_STATE_HTML) {
        m_header.CONTENT_LENGTH = std::to_string(m_http_data.size());
        complete_http_header();
        if(!f(m_header_data.data(), m_header_data.size())) {
            return false;
        }
        if(!f(m_http_data.data(), m_http_data.size())) {
            return false;
        }
    }
    m_state = HTTP_STATE_HEADER;
    m_header=http_header{};
    m_header_data.clear();
    m_http_data.clear();
    m_chunked = false;
    return true;
}

bool http::process_header(const char * data, const size_t data_length, size_t * processed,
                          std::function<bool(const char * data, const size_t data_length)> f) {
    debugprintf("Process header, %ld left", data_length);
    size_t pos{0};
    size_t n{0};
    do {
        char * buffer{nullptr};
        int buffer_size{0};
        bool have_newline{false};
        n = read_line( &buffer, &buffer_size, data + pos, data_length - pos, &have_newline);
        if(n > 0) {
            if(buffer != nullptr) {
                m_rest += std::string(buffer);
            }
            if(have_newline) {
                if(m_rest.size() == 0) {
                    debugprintf("Got end of header. type: %s", m_header.CONTENT_TYPE.data());
                    m_state = HTTP_STATE_CONTENT;
                    if(icontains(m_header.CONTENT_TYPE, std::string("text/html")))
                        m_state = HTTP_STATE_HTML;
                    else if(icontains(m_header.CONTENT_TYPE, std::string("text/xml")))
                        m_state = HTTP_STATE_HTML;

                    if(icontains(m_header.TRANSFER_ENCODING, std::string("chunked"))) {
                        debugprintf("Found chunked HTML format");
                        m_chunked = true;
                    }

                    m_http_data.clear();
                    m_missing = atoi(m_header.CONTENT_LENGTH.data());
                } else {
                    if(!parse_header_line(m_rest.data(), m_rest.size())) {
                        //Add unknown header lines directly to buffer
                        m_header_data += m_rest;
                        m_header_data += HTTP_NL;
                    }
                }
                //Clear rest
                m_rest = std::string{};
            }
            pos +=n;
            free(buffer);
        }
    } while ((n > 0) && (pos < data_length) && m_state == HTTP_STATE_HEADER);

    *processed = pos;
    return true;
}

bool http::process_plain(const char * data, const size_t data_length, size_t * processed,
                         std::function<bool(const char * data, const size_t data_length)> f) {
    debugprintf("Process plain, %ld left", data_length);
    size_t pos{0};
    if(m_state == HTTP_STATE_CONTENT) {
        //debugprintf("Missing %ld bytes", m_missing);
        if(!m_header_data.empty()) {
            debugprintf("Write ot header");
            complete_http_header();
            if (!f(m_header_data.data(), m_header_data.size())) {
                return false;
            }

            m_header = http_header{};
            m_header_data.clear();
        }
        if(pos < data_length) {
            size_t send = data_length - pos;
            if (send > m_missing) {
                send = m_missing;
            }
            if(!f(data + pos, send)) {
                return false;
            }
            m_missing -= send;
            pos +=send;
        }
        if(m_missing == 0) {
            m_state = HTTP_STATE_HEADER;
            if(pos <data_length) {
                debugprintf("Still something left. Call ourself");
                http::process(data + pos, data_length -pos, f);
            }
        }
    }
    if(m_state == HTTP_STATE_HTML) {
        //debugprintf("Missing %ld bytes", m_missing);
        size_t available = data_length - pos;
        if(available > 0) {
            int a;
            for(a = 0; (a < available) && (a < m_missing); a++) {
                m_http_data.push_back(*(data + (pos + a)));
            }
            m_missing -= a;
            available -= a;
            pos += a;

        }
        if(m_missing == 0) {
            debugprintf ("done reading data");
            debugprintf("Filter HTTP data");
            m_http_data = filter_http(m_http_data);
            m_state = HTTP_STATE_HEADER;

            m_header.CONTENT_LENGTH = std::to_string(m_http_data.size());
            complete_http_header();
            debugprintf("Write ot header");
            if(!f(m_header_data.data(), m_header_data.size())) {
                return false;
            }
            m_header = http_header{};
            m_header_data.clear();
            debugprintf("Write out HTML data");
            if(!f(m_http_data.data(), m_http_data.size())) {
                return false;
            }
            m_http_data.clear();
        }
    }
    *processed=pos;
    return true;
}

bool http::process_chunked(const char * data, const size_t data_length, size_t * processed,
                           std::function<bool(const char * data, const size_t data_length)> f) {
    size_t pos=0;
    //Write out header if not done so. In chunked mode we always can do so.
    if(!m_header_data.empty()) {
        debugprintf("Write ot header");
        complete_http_header();
        if (!f(m_header_data.data(), m_header_data.size())) {
            return false;
        }

        m_header = http_header{};
        m_header_data.clear();
    }
    do {
        if(m_chunk_missing == 0) {
            size_t n{0};
            //new chunk
            char * buffer{nullptr};
            int buffer_size{0};
            bool have_newline{false};
            n = read_line( &buffer, &buffer_size, data + pos, data_length - pos, &have_newline);
            if(n > 0) {
                if(buffer != nullptr) {
                    m_chunk_rest += std::string(buffer);
                }
                if(have_newline) {
                    m_chunk_missing = strtol(m_chunk_rest.data(), NULL, 16);
                    debugprintf("m_chunk_missing %ld",m_chunk_missing);
                    m_chunk_missing += 2; // add closing newline
                    m_chunk_rest.clear();
                }
                free (buffer);
            }
            pos += n;
        } else {
            size_t  n{0};
            size_t available = data_length - pos;
            if(available > m_chunk_missing) {
                for(int a=0; a < m_chunk_missing; a++) {
                    m_chunk_data.push_back((data+pos)[a]);
                }
                pos += m_chunk_missing;
                m_chunk_missing = 0;
            } else {
                for(int a=0; a < available; a++) {
                    m_chunk_data.push_back((data+pos)[a]);
                }
                pos += available;
                m_chunk_missing -= available;
            }
            if(m_chunk_missing == 0) {
                if(m_state == HTTP_STATE_HTML) {
                    m_chunk_data = filter_http(m_chunk_data);
                }
                char bf[100];
                sprintf(bf, "%lX", m_chunk_data.size() -2);
                std::string start = (bf);
                start += HTTP_NL;
                if(!f(start.data(), start.size()))
                    return false;
                if(!f(m_chunk_data.data(), m_chunk_data.size()))
                    return false;
                if(m_chunk_data.size() == 2) {
                    debugprintf("Found end of chunks");
                    m_chunked=false;
                    m_state=HTTP_STATE_HEADER;
                }
                m_chunk_data.clear();
            }
        }
    } while ((pos < data_length) && (m_state != HTTP_STATE_HEADER));
    *processed=pos;
    return true;
}

bool http::process(const char * data, const size_t data_length,
                   std::function<bool(const char * data, const size_t data_length)> f) {
    int pos{0};
    int n{0};

    if(data_length == 0) {
        flush_and_reset(f);
        return false;
    }

    if(m_state == HTTP_STATE_HEADER) {
        size_t taken =0;
        if(process_header(data+ pos, data_length-pos, &taken, f)) {
            pos +=taken;
        } else {
            return false;
        }
    }
    if (!m_chunked) {
        size_t taken =0;
        if(process_plain(data+ pos, data_length-pos, &taken, f)) {
            pos +=taken;
        } else {
            return false;
        }

    } else {
        size_t taken =0;
        if(process_chunked(data+ pos, data_length-pos, &taken, f)) {
            pos +=taken;
        } else {
            return false;
        }
    }
    if(pos < data_length) {
        debugprintf("Still something left. Call ourself");
        return http::process(data + pos, data_length - pos, f);
    }
    return true;
}




std::string http::filter_http(const std::string & http_data) {
    return std::string(http_data);
}


