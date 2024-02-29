#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/inotify.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/stat.h>

#ifndef SERVER_CLIENT_H
#define SERVER_CLIENT_H

#define EVENT_SIZE      (sizeof(struct inotify_event))
#define BUF_LEN         (1024 * (EVENT_SIZE + 16))

void monitor_folder(char **folders, int folders_size, char* client_id, int port, char* host);
void transfer_file(char* filename, char* filepath, char* client_id, int port, char* host);
char* get_client_identifier();

#endif //SERVER_CLIENT_H
