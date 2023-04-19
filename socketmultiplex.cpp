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
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>
#include <signal.h>

#include <vector>
#include "socketmultiplex.h"

#include <fcntl.h>
#include "debugprintf.h"

static void on_choke_nop(int, bool) {
    return;
}

static bool try_write(socket_helper &helper) {
    if(helper.writebuffer.size() > 0) {
        errno = 0;
        int n=write(helper.socket, helper.writebuffer.data(), helper.writebuffer.size());
        if(n == helper.writebuffer.size()) {
            helper.writebuffer.clear();
        } else if(n > 0) {
            std::vector<uint8_t> rest{};
            for(int a = 0; a < helper.writebuffer.size() - n; a ++) {
                rest.push_back(helper.writebuffer.data()[n + a]);
            }
            helper.writebuffer = rest;
        } else {
            if(errno == EAGAIN || errno == EWOULDBLOCK)
                return true;
            debugprintf("%d %s", n, strerror(errno));
            return false;
        }
    }
    debugprintf("%ld bytes left", helper.writebuffer.size());
    return true;
}

static int setup_multicast_socket (const char * ip, uint16_t port) {
    struct ip_mreq command;
    int loop = 1;
    int socket_descriptor;
    struct sockaddr_in sin;
    memset (&sin, 0, sizeof (sin));
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = htonl (INADDR_ANY);
    sin.sin_port = htons (port);
    if ( ( socket_descriptor = socket(PF_INET,
                                      SOCK_DGRAM, 0)) == -1) {
        perror ("socket()");
        return socket_descriptor;
    }
    /* Mehr Prozessen erlauben, denselben Port zu nutzen */
    loop = 1;
    if (setsockopt ( socket_descriptor,
                     SOL_SOCKET,
                     SO_REUSEADDR,
                     &loop, sizeof (loop)) < 0) {
        perror ("setsockopt:SO_REUSEADDR");
        close(socket_descriptor);
        return -1;
    }
    if(bind( socket_descriptor,
             (struct sockaddr *)&sin,
             sizeof(sin)) < 0) {
        perror ("bind");
        close(socket_descriptor);
        return -1;
    }
    /* Broadcast auf dieser Maschine zulassen */
    loop = 1;
    if (setsockopt ( socket_descriptor,
                     IPPROTO_IP,
                     IP_MULTICAST_LOOP,
                     &loop, sizeof (loop)) < 0) {
        perror ("setsockopt:IP_MULTICAST_LOOP");
        close(socket_descriptor);
        return -1;
    }
    /* Join the broadcast group: */
    command.imr_multiaddr.s_addr = inet_addr (ip);
    command.imr_interface.s_addr = htonl (INADDR_ANY);
    if (command.imr_multiaddr.s_addr == -1) {
        perror ("ist keine Multicast-Adresse\n");
        close(socket_descriptor);
        return -1;
    }
    if (setsockopt ( socket_descriptor,
                     IPPROTO_IP,
                     IP_ADD_MEMBERSHIP,
                     &command, sizeof (command)) < 0) {
        perror ("setsockopt:IP_ADD_MEMBERSHIP");
    }
    return socket_descriptor;
}

static int serversocket (uint16_t port) {
    int listen_fd;
    struct sockaddr_in serv_addr;

    listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if(listen_fd < 0) {
        perror( "ERROR opening socket");
        return listen_fd;
    }
    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(port);
    if (bind(listen_fd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
        perror( "ERROR binding socket");
        close(listen_fd);
        return -1;
    }
    if(listen(listen_fd,5)<0) {
        perror( "ERROR listening");
        close(listen_fd);
        return -1;
    }
    return listen_fd;
}


socketmultiplex::socketmultiplex():
    listener{},
    connections{},
    attempts{} {
    signal(SIGPIPE, SIG_IGN);
};

socketmultiplex::~socketmultiplex() {
    for(auto& helper: attempts) {
        close(helper.socket);
    }
    for(auto& helper: connections) {
        close(helper.socket);
    }
    for(auto& helper: listener) {
        close(helper.socket);
    }
}

int socketmultiplex::connect_port(const char * url, uint16_t port, std::function<bool(int socket)> f) {
    if(m_processing_attempts)
        debugprintf("called while processing");
    socket_helper h;
    struct sockaddr_in serv_addr;
    struct hostent * he;
    he=gethostbyname(url);
    if(he == NULL) {
        perror("ERROR getting host");
        return -1;
    }

    h.socket = socket(AF_INET, SOCK_STREAM,0);
    h.onChoke = on_choke_nop;
    if(h.socket < 0) {
        perror( "Error opening socket ");
        return h.socket;
    }
    int flags = fcntl(h.socket, F_GETFL, 0);
    flags |= O_NONBLOCK;
    //Switch to non blocking mode
    fcntl(h.socket, F_SETFL, flags);
    //Prepare address
    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    memcpy(&serv_addr.sin_addr, he->h_addr_list[0], he->h_length);
    serv_addr.sin_port = htons(port);
    //Try to connect
    if(connect(h.socket, (struct sockaddr*) &serv_addr, sizeof(serv_addr)) < 0) {
        if(errno != EINPROGRESS) {
            perror("ERROR on connect");

            close(h.socket);
            return -1;
        }
    }
    h.f = [this, f](int socket) {
        debugprintf("Connection ready?");
        //int flags = fcntl(socket, F_GETFL, 0);
        //flags &= ~O_NONBLOCK;
        //Switch to blocking mode
        //fcntl(socket, F_SETFL, flags);
        if(!f(socket)) {
            close(socket);
        }
        return false;
    };
    attempts.push_back(h);
    return h.socket;
}

int socketmultiplex::add_udp_mcast_listener(const char * url, uint16_t port, std::function<bool(int socket)> f) {
    int mcast_socket = setup_multicast_socket (url, port);
    if(mcast_socket <= 0) {
        return mcast_socket;
    }
    f(mcast_socket);
    return mcast_socket;
}

int socketmultiplex::add_port_listener(uint16_t listen_port, std::function<void(int socket)> f) {
    if(m_processing_listener)
        debugprintf("called while processing");
    listener_helper h;
    h.listen_port=listen_port;
    h.socket = serversocket(listen_port);
    h.f = f;
    if(h.socket >= 0)
        listener.push_back(h);
    return h.socket;
}

void socketmultiplex::remove_attempt(int socket) {
    if(m_processing_attempts)
        debugprintf("called while processing");
    debugprintf("remove attempt %d", socket);
    if(attempts.size() == 0)
        return;
    attempts.erase(std::remove_if(attempts.begin(), attempts.end(), [socket](socket_helper &h) {
        if(socket == h.socket) {
            return true;
        }
        return false;
    }),attempts.end());
}

void socketmultiplex::remove_port_listener(uint16_t listen_port) {
    if(m_processing_listener)
        debugprintf("called while processing");
    debugprintf("remove listener %d", listen_port);
    if(listener.size() == 0)
        return;
    listener.erase(std::remove_if(listener.begin(), listener.end(), [listen_port](listener_helper &h) {
        if(listen_port == h.listen_port) {
            close(h.socket);
            return true;
        }
        return false;
    }), listener.end());
}

void socketmultiplex::remove_socket_callback(int socket) {
    if(m_processing_connections)
        debugprintf("called while processing");
    debugprintf("close socket %d", socket);
    if(connections.size() == 0)
        return;
    connections.erase(std::remove_if(connections.begin(), connections.end(), [socket](socket_helper &h) {
        if(socket == h.socket) {
            //try to flush write. May succeed or not.
            try_write(h);
            close(socket);
            return true;
        }
        return false;
    }),connections.end());
}

int socketmultiplex::register_socket_callback(int socket, std::function<bool(int socket)> f) {
    if(m_processing_connections)
        debugprintf("called while processing");
    socket_helper h;
    h.socket=socket;
    h.onChoke = on_choke_nop;
    h.f=f;
    for(auto& helper: connections) {
        if(helper.socket == socket) {
            debugprintf("Overwriting existing connection");
            helper = h;
            return helper.socket;
        }
    }
    debugprintf("Add to connections");
    connections.push_back(h);
    return h.socket;
}

void socketmultiplex::add_socket_choke(uint16_t socket, std::function<void(int socket, bool enabled)> onChoke) {
    if(m_processing_connections)
        debugprintf("called while processing");
    for(auto& helper: connections) {
        if(helper.socket == socket) {
            helper.onChoke = onChoke;
        }
    }
}

ssize_t socketmultiplex::awrite(int socket, const void *data, size_t size, bool block) {
    for(auto& helper : connections) {
        if(helper.socket == socket) {
            debugprintf("add %ld bytes to writebuffer on %d", size, socket);
            for(int a = 0; a < size; a ++)
                helper.writebuffer.push_back(((uint8_t*) data)[a]);
            bool result = false;
            do {
                result = try_write(helper);
                if(errno == EAGAIN || errno == EWOULDBLOCK)
                    usleep(1000);
            } while(result && block && (helper.writebuffer.size() > 0));
            if((helper.writebuffer.size()> 1000) && (!helper.choke_requested)) {
                helper.onChoke(helper.socket, true);
                helper.choke_requested=true;
            }
            return size;
        }
    }
    errno=EBADF;
    return -1;
}

void socketmultiplex::choke(int socket, bool enable) {
    for(auto&helper:connections) {
        if(helper.socket==socket) {
            debugprintf("%schoke socket%d",(enable?"":"un"), socket);
            helper.choked=enable;
        }
    }

}

void socketmultiplex::handle_sockets(struct timeval tv) {
    int maxfd=0;
    int retval;
    fd_set read_fds;
    fd_set write_fds;

    FD_ZERO(&read_fds);
    FD_ZERO(&write_fds);
    //Add attempting sockets to select
    for(auto& helper: attempts) {
        if(maxfd <= helper.socket)
            maxfd = helper.socket +1;

        FD_SET(helper.socket, &write_fds);
    }
    //Add listen sockets to select
    for(auto& helper: listener) {
        if(maxfd <= helper.socket)
            maxfd = helper.socket +1;

        FD_SET(helper.socket, &read_fds);
    }

    //Add connection sockets to select
    for(auto& helper: connections) {
        if(!helper.choked) {
            if(maxfd <= helper.socket)
                maxfd = helper.socket +1;

            FD_SET(helper.socket, &read_fds);
        }
        //Add write sockets to select
        if(!helper.writebuffer.empty()) {
            debugprintf("Something to write on %d", helper.socket);
            if(maxfd <= helper.socket)
                maxfd = helper.socket +1;

            FD_SET(helper.socket, &write_fds);
        }
    }

    retval=select(maxfd, &read_fds, &write_fds, NULL, &tv);
    if(retval == -1) {
        perror("ERROR on select");
    } else if(retval) {
        std::vector<int> closing{};
        //Writing first
        m_processing_attempts=true;
        for(int sock=0; sock < maxfd; sock ++) {
            if(FD_ISSET(sock, &write_fds)) {
                for(auto& helper: connections) {
                    if(helper.socket == sock) {
                        debugprintf("Ready to write: %d", sock);
                        if(!try_write(helper)) {
                            closing.push_back(sock);
                        }
                        if((helper.writebuffer.size()<= 1000) && (helper.choke_requested)) {
                            helper.onChoke(helper.socket, false);
                            helper.choke_requested=false;
                        }
                        break;
                    }
                }
            }
        }
        for(auto sock: closing) {
            remove_socket_callback(sock);
        }
        closing.clear();
        m_processing_attempts=false;
        //Attempts first
        m_processing_attempts=true;
        for(int sock=0; sock < maxfd; sock ++) {
            if(FD_ISSET(sock, &write_fds)) {
                for(auto& helper: attempts) {
                    if(helper.socket == sock) {
                        debugprintf("Socket connected.");
                        helper.f(helper.socket);
                        closing.push_back(helper.socket);
                        break;
                    }
                }
            }
        }
        for(auto sock: closing) {
            remove_attempt(sock);
        }
        m_processing_attempts=false;
        m_processing_connections=true;
        closing.clear();
        //Connections first, as listener may alter connections.
        for(int sock=0; sock < maxfd; sock ++) {
            if(FD_ISSET(sock, &read_fds)) {
                for(auto& helper: connections) {
                    if(helper.socket == sock) {
                        //debugprintf("Socket ready to read.");
                        if(!helper.f(helper.socket)) {
                            closing.push_back(helper.socket);
                        }
                        break;
                    }
                }
            }
        }
        for(auto sock: closing) {
            remove_socket_callback(sock);
        }
        m_processing_connections=false;
        m_processing_listener=true;
        closing.clear();

        //Accept any new connections?
        for(auto& helper: listener) {
            if(FD_ISSET(helper.socket, &read_fds)) {
                int newsock;
                socklen_t clilen;
                struct sockaddr_in cli_addr;

                clilen = sizeof(cli_addr);
                newsock = accept(helper.socket, (struct sockaddr *) &cli_addr, &clilen);
                if (newsock < 0) {
                    perror("ERROR on accept");
                } else {
                    int flags = fcntl(newsock, F_GETFL, 0);
                    flags |= O_NONBLOCK;
                    //Switch to non blocking mode
                    fcntl(newsock, F_SETFL, flags);
                    //Add to connections
                    debugprintf("call callback");
                    helper.f(newsock);
                }
            }
        }
        m_processing_listener=false;
    } else {
//        debugprintf("Timeout");
    }
}

