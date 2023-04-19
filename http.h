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

#ifndef __HTTP_H
#define __HTTP_H

#include <functional>
#include <string>

#include "tunnel_filter.h"

enum http_state {
    HTTP_STATE_HEADER,
    HTTP_STATE_CONTENT,
    HTTP_STATE_HTML
};

struct http_header {
    std::string HOST{};
    std::string CONTENT_TYPE{};
    std::string CONTENT_LENGTH{};
    std::string TRANSFER_ENCODING{};
};

class http : public tunnel_filter {
public:
    http(std::string realHost);
    virtual ~http();

    bool process(const char * data, const size_t data_length,
                 std::function<bool(const char * data, const size_t data_length)> f) override;
private:
    virtual std::string filter_http(const std::string & http_data);
    bool flush_and_reset(std::function<bool(const char * data, const size_t data_length)> f);
    bool parse_header_line(const char * line, size_t line_length);
    void complete_http_header();
    bool process_header(const char * data, const size_t data_length, size_t * processed,
                        std::function<bool(const char * data, const size_t data_length)> f);
    bool process_plain(const char * data, const size_t data_length, size_t * processed,
                       std::function<bool(const char * data, const size_t data_length)> f);
    bool process_chunked(const char * data, const size_t data_length, size_t * processed,
                         std::function<bool(const char * data, const size_t data_length)> f);
    http_state m_state{HTTP_STATE_HEADER};
    bool m_chunked{false};
    size_t m_missing{0};
    size_t m_chunk_missing{0};
    http_header m_header{};
    std::string m_header_data{};
    std::string m_http_data{};
    //std::vector<char> m_chunk_data{};
    std::string m_chunk_data{};
    std::string m_chunk_rest{};
    std::string m_rest{}; //Remmber anything not yet a line / tag.
    std::string m_realHost;
};



#endif
