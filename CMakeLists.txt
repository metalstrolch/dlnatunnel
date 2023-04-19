cmake_minimum_required(VERSION 3.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FindPkgConfig)

project(dlnatunnel_project)

file(GLOB sources collector.cpp dlna_filter.cpp http.cpp mplex.cpp server.cpp socketmultiplex.cpp ssdp.cpp stringtoken.cpp tunnel.cpp tunnel_filter.cpp uri.cpp)
file(GLOB header collector.h dlna_filter.h debugprintf.h http.h mplex.h socketmultiplex.h ssdp.h stringtoken.h tunnel.h tunnel_filter.h uri.h)

include_directories(.)

add_executable (dlnatunnel ${sources} ${header})
install(TARGETS dlnatunnel  DESTINATION bin)
