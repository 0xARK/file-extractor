#include <signal.h>
#include "server.h"

struct Thread_params{
    int client_socket;
    int id;
};

int main(int argc, char** argv) {
    uint16_t port = 3000;
    char* listener = NULL;
    int opt;

    opterr = 0;

    // handle provided options to the program using getopt
    while ((opt = getopt(argc, argv, "p:l:")) != -1) {
        switch (opt) {
            case 'l':
                listener = optarg;
                break;
            case 'p':
                port = strtol(optarg, NULL, 10);
                // reset port to default value in case of conversion error
                if (port == 0) port = 3000;
                break;
            case '?':
                // handle required and unknown options
                if (optopt == 'l' || optopt == 'p') {
                    fprintf(stderr, "Option -%c requires an argument.\n", optopt);
                } else {
                    fprintf(stderr, "Unknown option `-%c'.\n", optopt);
                }
                return 1;
            default:
                abort();
        }
    }

    printf("port = %d, listen = %s\n", port, listener);
    start_server(port, listener);

    printf("Finished\n");
    return 0;
}

/*
 * Allows to specify if we want to continue to monitor folders or not
 */
static volatile sig_atomic_t listening = 1;

/**
 * Allows to intercept sigint signal to stop watching folders properly
 */
void sigint_interceptor() {
    listening = 0;
}

void start_server(int port, char* listener) {
    int sock_server;
    int bind_server;

    sock_server = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_server < 0) {
        perror("socket");
        exit(1);
    }

    struct sockaddr_in server;
    server.sin_addr.s_addr = inet_addr(listener);
    server.sin_family = AF_INET;
    server.sin_port = htons(port);

    bind_server = bind(sock_server, (const struct sockaddr *)&server, sizeof(server));
    if (bind_server < 0) {
        perror("bind");
        exit(1);
    }

    // listen for up to 10 connections
    if (listen(sock_server, 10) == 0) {
        printf("Listening...\n");
    }

    struct sockaddr_in client[10];
    socklen_t addr_len = sizeof(*client);

    int i = 0;
    int client_sock[10];
    pthread_t Thread[10];

    // handle sigint signals to stop accept() and exit program more smoothly
    struct sigaction int_handler = {.sa_handler=sigint_interceptor};
    sigaction(SIGINT,&int_handler,0);

    while (listening) {
        client_sock[i] = accept(sock_server, (struct sockaddr *)&client[i], &addr_len);
        struct Thread_params param;
        param.client_socket=client_sock[i];
        param.id=i;
        pthread_create(&Thread[i],NULL,client_file_handle,&param);
        pthread_join(Thread[i],NULL);

        i++;
    }

    close(sock_server);
    printf("finished\n");
}

void* client_file_handle(void* args) {
    struct Thread_params* params = (struct Thread_params*) args;
    printf("Thread n° %d \n",params->id);
    char filename[2000];
    recv(params->client_socket, &filename, sizeof(filename), 0);

    int length = 0;
    read(params->client_socket, &length, sizeof(length));

    char* filepath;

    filepath = malloc(strlen(filename) + sizeof("reception/"));
    strcpy(filepath, "reception/");
    strcat(filepath, filename);

    FILE* f = fopen(filepath, "w");

    char* data = malloc(length);
    int i = 0;
    while (i < length) {
        recv(params->client_socket, &data[i], sizeof(data[i]), 0);
        if (&data[i] == NULL)
            break;
        fwrite(&data[i], 1, 1, f);
        i++;
    }

    fclose(f);
    close(params->client_socket);

    free(filepath);
    free(data);

    return NULL;
}