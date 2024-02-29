#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <string.h>
#include <signal.h>

#ifndef SERVER_SERVER_H
#define SERVER_SERVER_H

void start_server(int port, char* listener);
void client_file_handle(int client_socket);

#endif //SERVER_SERVER_H
