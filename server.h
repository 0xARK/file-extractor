#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <string.h>
#include <pthread.h>

#ifndef SERVER_SERVER_H
#define SERVER_SERVER_H

void start_server(int port, char* listener);
void* client_file_handle(void* args);

#endif //SERVER_SERVER_H
