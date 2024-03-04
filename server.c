#include "server.h"

/**
 * This server allows to listen for clients sharing new files created on their system. These files, and some others
 * information like the connected client id, are shared through a connection secured by ssl, and then stored on the
 * server disk in separated folders. Each folder represent a client, named according to the shared client id. An
 * integrity check is also performed by generating a sha256 checksum of the freshly written file, and compared to the
 * original file checksum, also shared by the client.
 *
 * @param argc - arguments count
 * @param argv - arguments values
 * @return - exit status code
 */
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

/**
 * This method allows to create the server ssl context
 *
 * @return - the global ssl context of this server
 */
SSL_CTX* create_context() {
    const SSL_METHOD* method;
    SSL_CTX* ctx;

    method = TLS_server_method();

    ctx = SSL_CTX_new(method);
    if (!ctx) {
        perror("Unable to create SSL context");
        exit(EXIT_FAILURE);
    }

    return ctx;
}

/**
 * This method allows to configure the ssl server context with the private key and public certificate
 *
 * @param ctx - the global ssl context of this server
 */
void configure_context(SSL_CTX *ctx) {
    // set the key and certificate
    if (SSL_CTX_use_certificate_file(ctx, "cert.pem", SSL_FILETYPE_PEM) <= 0) {
        perror("Can not use certificate file");
        exit(EXIT_FAILURE);
    }

    if (SSL_CTX_use_PrivateKey_file(ctx, "key.pem", SSL_FILETYPE_PEM) <= 0 ) {
        perror("Can not use private key file");
        exit(EXIT_FAILURE);
    }

    if (!SSL_CTX_check_private_key(ctx)) {
        perror("Private key and public certificate doesn't match");
        exit(EXIT_FAILURE);
    }
}

/*
 * Allows to specify if we want to continue to monitor folders or not
 */
static volatile sig_atomic_t listening = 1;

/**
 * This method to intercept sigint signal to stop listening for client properly
 */
void int_modifier() {
    listening = 0;
}

int create_socket(int port, char* listener) {
    int passive_sock, bind_sock;
    struct sockaddr_in server_addr;

    passive_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (passive_sock < 0) {
        perror("Unable to create socket");
        exit(EXIT_FAILURE);
    }

    // initialize server socket struct
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(listener);
    server_addr.sin_port = htons(port);

    // bind server socket to listener and port
    bind_sock = bind(passive_sock, (const struct sockaddr *)&server_addr, sizeof(server_addr));
    if (bind_sock < 0) {
        perror("Unable to bind socket");
        exit(EXIT_FAILURE);
    }

    // listen for client connection, up to 1024 pending request queue
    if (listen(passive_sock, 1024) < 0) {
        perror("Unable to listen socket");
        exit(EXIT_FAILURE);
    }
    printf("Listening...\n");

    return passive_sock;
}

void start_server(int port, char* listener) {
    int server_sock, client_sock;
    socklen_t addr_len;
    SSL_CTX* ctx;
    struct sockaddr_in client_addr;

    // ignore broken pipe signals
    struct sigaction pipe_handler = {.sa_handler=SIG_IGN};
    sigaction(SIGPIPE, &pipe_handler, 0);

    // initialize SSL context and server socket
    ctx = create_context();
    configure_context(ctx);
    server_sock = create_socket(port, listener);

    // handle sigint signals to stop accept() and exit program more smoothly
    struct sigaction int_handler = {.sa_handler=int_modifier};
    sigaction(SIGINT,&int_handler,0);

    // while ctrl+c is not pressed, accept client connections
    while (listening) {
        addr_len = sizeof(struct sockaddr_in);
        SSL* ssl;
        client_sock = accept(server_sock, (struct sockaddr *)&client_addr, &addr_len);

        ssl = SSL_new(ctx);
        SSL_set_fd(ssl, client_sock);

        if (SSL_accept(ssl) <= 0) {
            // don't exit in order to quit program smoothly in case of SIGINT interception
            perror("Accept connection failed");
            close(client_sock);
            SSL_shutdown(ssl);
            SSL_free(ssl);
            SSL_CTX_free(ctx);
        } else {
            if (fork() == 0) {
                client_file_handle(ssl);
                SSL_shutdown(ssl);
                SSL_free(ssl);
                SSL_CTX_free(ctx);
                close(client_sock);
                exit(EXIT_SUCCESS);
            }
        }
    }

    close(server_sock);
}

char* get_file_path(char* file_name, char* client_id, int is_corrupted) {
    char* dest_path = "./client-files/";
    char* corrupted = "corrupted_";
    char* folder_path;
    char* file_path;
    struct stat st = {0};

    // create client folder if it doesn't exist
    folder_path = (char*) malloc(strlen(dest_path) + strlen(client_id) + 1); // add 1 for null byte
    strcpy(folder_path, dest_path);
    strcat(folder_path, client_id);
    if (stat(folder_path, &st) == -1) {
        printf("Client folder %s does not exist, creating it\n", folder_path);
        mkdir(folder_path, 0744);
    }
    free(folder_path);

    // build the filepath where we want to write the file content
    int corrupted_length = is_corrupted ? (int) strlen(corrupted) : 0;
    file_path = (char*) malloc(strlen(dest_path) + strlen(client_id) + strlen(file_name) + corrupted_length + 1 + 1); // add 1 for / and 1 for null byte
    strcpy(file_path, dest_path);
    strcat(file_path, client_id);
    strcat(file_path, "/");
    if (is_corrupted) strcat(file_path, corrupted);
    strcat(file_path, file_name);

    return file_path;
}

void sha256sum(char* path, char output[65]) {
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256_CTX sha256;
    const int bufSize = 32768;
    int bytes_read;
    int i;

    // open file to read content from, in order to generate its checksum
    FILE* file = fopen(path, "rb");
    if (file == NULL) {
        perror("file open for sha256sum failed");
        exit(EXIT_FAILURE);
    }

    // initialize sha256 hash
    SHA256_Init(&sha256);
    char* buffer = (char*) malloc(bufSize);
    if (buffer == NULL) {
        perror("Memory allocation for sha256sum failed");
        exit(EXIT_FAILURE);
    }

    // update sha256 hash with read content from opened file
    while ((bytes_read = (int) fread(buffer, 1, bufSize, file))) {
        SHA256_Update(&sha256, buffer, bytes_read);
    }
    fclose(file);
    free(buffer);

    // finalize sha256 hash
    SHA256_Final(hash, &sha256);

    // convert unsigned char to char string
    for (i = 0; i < SHA256_DIGEST_LENGTH; i++) {
        sprintf(output + (i * 2), "%02x", hash[i]);
    }
    output[64] = 0;
}

void client_file_handle(SSL* ssl) {
    // receive client id length from client
    uint16_t client_id_length;
    SSL_read(ssl, &client_id_length, sizeof(client_id_length));
    // receive client id from client
    char client_id[client_id_length + 1]; // add 1 for null byte
    SSL_read(ssl, client_id, client_id_length + 1); // add 1 for null byte

    // get filename length from client
    uint16_t file_name_length;
    SSL_read(ssl, &file_name_length, sizeof(file_name_length));
    // get filename from client
    char file_name[file_name_length + 1]; // add 1 for null byte
    SSL_read(ssl, &file_name, file_name_length + 1); // add 1 for null byte

    // get file length from client
    int length = 0;
    SSL_read(ssl, &length, sizeof(length));

    // get file checksum from client
    char original_sha256_checksum[65]; // 64 + 1 for null byte
    SSL_read(ssl, &original_sha256_checksum, sizeof(original_sha256_checksum));

    printf("\nClient %s has sent a file\nFile to receive is %s, with a length of %d byte(s)\n",
           client_id, file_name, length);
    printf("Original file cheksum: %s\n", original_sha256_checksum);

    char* file_path = get_file_path(file_name, client_id, 0);

    // get file content from client and write it on disk in a file
    FILE* f = fopen(file_path, "w");
    char* file_data = malloc(length);
    int i = 0;
    while (i < length) {
        SSL_read(ssl, &file_data[i], sizeof(file_data[i]));
        if (&file_data[i] == NULL) break;
        fwrite(&file_data[i], 1, 1, f);
        i++;
    }
    fclose(f);
    free(file_data);

    // get sha256 checksum of freshly written file, in order to compare it to received checksum received from client
    char sha256_checksum[65]; // 64 + 1 for null byte
    sha256sum(file_path, sha256_checksum);
    printf("Written file checksum: %s\n", sha256_checksum);

    if (strcmp(sha256_checksum, original_sha256_checksum) == 0) {
        printf("Destination path is: %s\n", file_path);
        printf("File received successfully\n");
    } else {
        // rename file if checksum are not corresponding
        char* corrupted_file_path = get_file_path(file_name, client_id, 1);
        printf("Destination path is: %s\n", corrupted_file_path);
        printf("Received file is corrupted\n");
        rename(file_path, corrupted_file_path);
        free(corrupted_file_path);
    }

    free(file_path);
}