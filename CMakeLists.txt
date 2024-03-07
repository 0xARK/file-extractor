cmake_minimum_required(VERSION 3.15)
project(client C)
project(server C)

set(CMAKE_C_STANDARD 11)

add_executable(client client/client.c client/client.h)
add_executable(server server/server.c server/server.h)

find_package(OpenSSL REQUIRED)
target_link_libraries(client OpenSSL::SSL)
target_link_libraries(server OpenSSL::SSL)

add_custom_command(TARGET client POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:client> ../client
)
add_custom_command(TARGET server POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:server> ../server
)