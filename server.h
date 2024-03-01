#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <string.h>
#include <signal.h>
#include <openssl/ssl.h>

#ifndef SERVER_SERVER_H
#define SERVER_SERVER_H

void configure_context(SSL_CTX *ctx);
SSL_CTX* create_context();
int create_socket(int port, char* listener);
void start_server(int port, char* listener);
void client_file_handle(int client_socket);

#endif //SERVER_SERVER_H
