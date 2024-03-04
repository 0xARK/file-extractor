#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/inotify.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <openssl/ssl.h>

#ifndef SERVER_CLIENT_H
#define SERVER_CLIENT_H

#define EVENT_SIZE      (sizeof(struct inotify_event))
#define BUF_LEN         (1024 * (EVENT_SIZE + 16))

void monitor_folder(char **folders, int folders_size, char* client_id, int port, char* host);
void transfer_file(char* filename, char* filepath, char* client_id, int port, char* host);
void sha256sum(char* path, char output[65]);

int create_socket(int port, char* host);

char* get_client_identifier();
char* get_file_path(char* file_name, char* folder_path);

SSL_CTX* create_context();

#endif //SERVER_CLIENT_H
