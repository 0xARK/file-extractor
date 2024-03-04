cmake_minimum_required(VERSION 3.15)
project(client C)
project(server C)

set(CMAKE_C_STANDARD 11)

add_executable(client client.c client.h)
add_executable(server server.c server.h)

find_package(OpenSSL REQUIRED)
target_link_libraries(client OpenSSL::SSL)
target_link_libraries(server OpenSSL::SSL)