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
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = inet_addr(listener);
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
}

void* client_file_handle(void* args) {
    struct Thread_params* params = (struct Thread_params*) args;
    printf("Thread n° %d \n",params->id);

    // receive client id length from client
    uint16_t client_id_length;
    recv(params->client_socket, &client_id_length, sizeof(client_id_length), 0);
    // receive client id from client
    char client_id[client_id_length + 1]; // add 1 for null byte
    recv(params->client_socket, client_id, client_id_length + 1, 0); // add 1 for null byte

    // get filename length from client
    uint16_t filename_length;
    recv(params->client_socket, &filename_length, sizeof(filename_length), 0);
    // get filename from client
    char filename[filename_length + 1]; // add 1 for null byte
    recv(params->client_socket, &filename, filename_length + 1, 0); // add 1 for null byte

    // get file length from client
    int length = 0;
    recv(params->client_socket, &length, sizeof(length), 0);

    // build the filepath where we want to write the file content
    // todo : change filepath with client id + check if folder exists
    char* dest_path;
    dest_path = malloc(strlen(filename) + sizeof("./reception/"));
    strcpy(dest_path, "./reception/");
    strcat(dest_path, filename);
    printf("dest path: %s\n", dest_path);

    // get file content from client and write it on disk in a file
    FILE* f = fopen(dest_path, "w");
    char* file_data = malloc(length);
    int i = 0;
    while (i < length) {
        recv(params->client_socket, &file_data[i], sizeof(file_data[i]), 0);
        if (&file_data[i] == NULL)
            break;
        fwrite(&file_data[i], 1, 1, f);
        i++;
    }

    fclose(f);
    close(params->client_socket);

    // free allocated memory
    free(dest_path);
    free(file_data);

    return NULL;
}