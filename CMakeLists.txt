cmake_minimum_required(VERSION 3.27)
project(client C)
project(server C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY bin)

add_executable(client client.c)
add_executable(server server.c)
