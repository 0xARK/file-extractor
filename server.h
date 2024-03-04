#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <string.h>
#include <signal.h>
#include <openssl/ssl.h>
#include <sys/stat.h>

#ifndef SERVER_SERVER_H
#define SERVER_SERVER_H

void configure_context(SSL_CTX *ctx);
void start_server(int port, char* listener);
void client_file_handle(SSL* ssl);
void sha256sum(char* path, char output[65]);

int create_socket(int port, char* listener);

char* get_file_path(char* file_name, char* client_id, int is_corrupted);

SSL_CTX* create_context();

#endif //SERVER_SERVER_H
