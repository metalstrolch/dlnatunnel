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

/* A simple server in the internet domain using TCP
   The port number is passed as an argument */
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <errno.h>
#include <string.h>
#include "debugprintf.h"

#include "socketmultiplex.h"
#include "tunnel.h"
#include "collector.h"

static volatile bool running = true;
static void intHandler(int) {
    fprintf(stderr, "About to quit\n");
    running=false;
}

static ssdp s_ssdp{};

class dlnatunnel {
public:
    dlnatunnel() {
    };
    ~dlnatunnel() {
        if(m_px != nullptr)
            delete m_px;
        if(m_tun != nullptr);
        delete m_tun;
        if(m_col != nullptr);
        delete m_col;
    };
    void kill() {
        if(m_tun != nullptr);
        delete m_tun;
        if(m_col != nullptr);
        delete m_col;

        m_tun=nullptr;
        m_col=nullptr;
    };
    socketmultiplex * m_px{nullptr};
    tunnel * m_tun{nullptr};
    collector * m_col{nullptr};
};

int main(int argc, char *argv[]) {
    const char* host = nullptr;
    const char* port = nullptr;
    bool server=false;
    if ((argc > 2) && (argc < 3)) {
        fprintf(stderr,"ERROR, no host and port provided\n");
        exit(1);
    } else if (argc < 2) {
        fprintf(stderr,"ERROR, no port provided\n");
        exit(1);
    }
    if(argc==2) {
        server = true;
        port = argv[1];
    }
    if(argc==3) {
        server = false;
        host = argv[1];
        port = argv[2];
    }
    signal(SIGINT, intHandler);
    dlnatunnel dtun{};
    dtun.m_px = new socketmultiplex{};

    if(! server) {
        dtun.m_px->connect_port(host, atoi(port), [&dtun] (int port_socket) {
            fprintf(stderr, "connect connection %d\n", port_socket);
            // Get my ip address and port
            char myIP[16];
            unsigned int myPort;
            struct sockaddr_in  my_addr;
            bzero(&my_addr, sizeof(my_addr));
            socklen_t len = sizeof(my_addr);
            getsockname(port_socket, (struct sockaddr *) &my_addr, &len);
            inet_ntop(AF_INET, &my_addr.sin_addr, myIP, sizeof(myIP));
            myPort = ntohs(my_addr.sin_port);
            errorprintf("%s:%d", myIP, myPort);

            dtun.m_col = new collector(std::string(myIP));
            dtun.m_col->use_px(dtun.m_px);
            dtun.m_tun = new tunnel(dtun.m_px, port_socket, [&dtun](tunnel * tn) {
                fprintf(stderr, "TUNNEL ready\n");
                dtun.m_col->use_tunnel(dtun.m_tun);

                return true;
            });

            dtun.m_px->register_socket_callback(port_socket, [&dtun] (int port_socket) {
                if(!dtun.m_tun->receive(port_socket)) {
                    running=false;
                    dtun.kill();
                    return false;
                }
                return true;
            });
            dtun.m_tun->run();
            return true;
        });
    } else {
        std::function<void(int)> f = [&dtun] (int port_socket) {
            fprintf(stderr, "port forward connection\n");
            dtun.m_tun = new tunnel(dtun.m_px, port_socket, [](tunnel * tn) {
                fprintf(stderr, "TUNNEL ready\n");
                return;
            });
            dtun.m_px->register_socket_callback(port_socket, [&dtun] (int port_socket) {
                if(!dtun.m_tun->receive(port_socket)) {
                    dtun.kill();
                    return false;
                }
                return true;
            });
            dtun.m_tun->run();
        };
        dtun.m_px->add_port_listener(atoi(argv[1]), f);
    }
    while(running) {
        struct timeval tv;
        tv.tv_sec=1;
        tv.tv_usec=0;
        dtun.m_px->handle_sockets(tv);
    }
    dtun.m_px->remove_port_listener(atoi(argv[1]));
    return 0;
}

