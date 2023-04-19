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

#include "http.h"
#include <string.h>
#include <unistd.h>
#include <stdio.h>

const char header[] = {
    "HTTP/1.1 400 Bad Request\r\n"
    "Connection: close\r\n"
    "Content-Length: 196\r\n"
    "Content-Type: text/html\r\n"
    "\r"
};
const char data[] = {
    "\n"
    "<HTML><HEAD><TITLE>400 Bad Request (ERR_INVALID_HOSTHEADER)</TITLE></HEAD><BODY><H1>400 Bad Request</H1><BR>ERR_INVALID_HOSTHEADER<HR><B>Webserver</B> Sun, 30 Apr 2023 19:51:43 GMT</BODY></HTML>\r\nBUMS"
};


const char chunked[] = {
    "HTTP/1.1 200 OK\r\n"
    "DATE: Wed, 10 May 2023 11:39:51 GMT\r\n"
    "SERVER: FRITZ!Box 7490 (UI) UPnP/1.0 AVM FRITZ!Box 7490 (UI) 113.07.29\r\n"
    "CONNECTION: keep-alive\r\n"
    "CONTENT-TYPE: text/xml; charset=\"utf-8\"\r\n"
    "EXT:\r\n"
    "Transfer-Encoding: chunked\r\n"
    "\r\n"
    "7f4\r\n"
    "<?xml version=\"1.0\"?>\n"
    " <s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\">\n"
    "<s:Body>\n"
    "<u:BrowseResponse xmlns:u=\"urn:schemas-upnp-org:service:ContentDirectory:1\">\n"
    "<Result>&lt;DIDL-Lite xmlns:dc=&quot;http://purl.org/dc/elements/1.1/&quot; xmlns:upnp=&quot;urn:schemas-upnp-org:metadata-1-0/upnp/&quot; xmlns=&quot;urn:schemas-upnp-org:metadata-1-0/DIDL-Lite/&quot;&gt;&lt;container id=&quot;4:cont1:20:0:0:&quot; parentID=&quot;0&quot; restricted=&quot;0&quot; searchable=&quot;1&quot; childCount=&quot;6&quot; &gt;&lt;dc:title&gt;Musik&lt;/dc:title&gt; &lt;upnp:class&gt;object.container&lt;/upnp:class&gt; &lt;/container&gt;&lt;container id=&quot;4:cont1:90:0:0:&quot; parentID=&quot;0&quot; restricted=&quot;0&quot; searchable=&quot;1&quot; childCount=&quot;2&quot; &gt;&lt;dc:title&gt;Bilder&lt;/dc:title&gt; &lt;upnp:class&gt;object.container&lt;/upnp:class&gt; &lt;/container&gt;&lt;container id=&quot;4:cont2:120:0:0:&quot; parentID=&quot;0&quot; restricted=&quot;0&quot; searchable=&quot;1&quot; childCount=&quot;2&quot; &gt;&lt;dc:title&gt;Filme&lt;/dc:title&gt; &lt;upnp:class&gt;object.container&lt;/upnp:class&gt; &lt;/container&gt;&lt;container id=&quot;4:cont2:150:0:0:&quot; parentID=&quot;0&quot; restricted=&quot;0&quot; searchable=&quot;1&quot; childCount=&quot;0&quot; &gt;&lt;dc:title&gt;Internetradio&lt;/dc:title&gt; &lt;upnp:class&gt;object.container&lt;/upnp:class&gt; &lt;/container&gt;&lt;container id=&quot;4:cont2:160:0:0:&quot; parentID=&quot;0&quot; restricted=&quot;0&quot; searchable=&quot;1&quot; childCount=&quot;0&quot; &gt;&lt;dc:title&gt;Podcasts&lt;/dc:title&gt; &lt;upnp:class&gt;object.container&lt;/upnp:class&gt; &lt;/container&gt;&lt;container id=&quot;4:cont2:170:0:0:&quot; parentID=&quot;0&quot; restricted=&quot;0&quot; searchable=&quot;1&quot; childCount=&quot;1&quot; &gt;&lt;dc:title&gt;Datei-Index&lt;/dc:title&gt; &lt;upnp:class&gt;object.container&lt;/upnp:class&gt; &lt;/container&gt;&lt;/DIDL-Lite&gt;</Result>\r\n"
    "23\r\n"
    "\n"
    "<NumberReturned>6</NumberReturned>\r\n"
    "1f\r\n"
    "\n"
    "<TotalMatches>6</TotalMatches>\r\n"
    "17\r\n"
    "\n"
    "<UpdateID>0</UpdateID>\r\n"
    "2c\r\n"
    "</u:BrowseResponse>\n"
    "</s:Body>\n"
    "</s:Envelope>\r\n"
    "0\r\n"
    "BUMS"
};


int main (int argc, char ** argv) {
    http h{std::string{"192.168.2.100:100"},std::string{"10.0.0.1:80"}};
#if 0
    h.process(header, strlen(header), [] (const char * buffer, const size_t buffer_size) {
        fwrite(buffer, buffer_size, 1, stdout);
        return true;
    });
    h.process(data, strlen(data), [] (const char * buffer, const size_t buffer_size) {
        fwrite(buffer, buffer_size, 1, stdout);
        return true;
    });
#else
    h.process(chunked, strlen(chunked), [] (const char * buffer, const size_t buffer_size) {
        fwrite(buffer, buffer_size, 1, stdout);
        return true;
    });
#endif
    h.process(nullptr, 0, [] (const char * buffer, const size_t buffer_size) {
        fwrite(buffer, buffer_size, 1, stdout);
        return true;
    });
}
