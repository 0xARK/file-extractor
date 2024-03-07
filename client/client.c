#include "client.h"

/**
 * This client allows to monitor a list of folder for newly created files. The new files are then sent to the server
 * through a connection secured by ssl, allowing to ensure confidentiality of data. An integrity check is also performed
 * by generating a sha256 checksum of files and transmit them to the server, in order to detect corrupted data. This client
 * is identified to the server thanks to a unique id read from /etc/machine-id.
 *
 * @param argc - arguments count
 * @param argv - arguments value
 * @return - exit status code
 */
int main(int argc, char** argv) {
    uint16_t port = 3000;
    char* host = "127.0.0.1";
    int i;
    int opt;
    int folders_size;
    char** folders = NULL;
    int argv_index;
    char* client_id = NULL;

    opterr = 0;

    // handle provided options to the program using getopt
    while ((opt = getopt(argc, argv, "p:h:")) != -1) {
        switch (opt) {
            case 'h':
                host = optarg;
                break;
            case 'p':
                port = strtol(optarg, NULL, 10);
                // reset port to default value in case of conversion error
                if (port == 0) port = 3000;
                break;
            case '?':
                // handle required and unknown options
                if (optopt == 'h' || optopt == 'p') {
                    fprintf(stderr, "Option -%c requires an argument.\n", optopt);
                } else {
                    fprintf(stderr, "Unknown option `-%c'.\n", optopt);
                }
                return 1;
            default:
                abort();
        }
    }

    printf("port = %d, host = %s\n", port, host);

    /*
     * The following section allows to process the folders paths provided in parameters.
     * These paths are stored in a dynamic array in order to not limit the user to a specific number of folder to monitor.
     * The length of the path string is also dynamic, to not restrict user too.
     */

    // folders size = total arg count - last processed arg by getopt
    folders_size = argc - optind;
    folders = (char**) malloc(folders_size * sizeof(char*));
    if (folders == NULL) {
        perror("Memory allocation for folders failed");
        exit(EXIT_FAILURE);
    }

    // retrieve the list of folders to monitor
    for (i = 0; i < folders_size; i++) {
        // argv index is the index of the next folder path, given in args, to process
        // argv_index = last processed arg by getopt + i
        argv_index = optind + i;
        folders[i] = (char*) malloc(strlen(argv[argv_index]));
        if (folders[i] == NULL) {
            perror("Memory allocation for folders[i] failed");
            exit(EXIT_FAILURE);
        }
        strcpy(folders[i], argv[argv_index]);
    }

    // get client id and start monitoring folders
    client_id = get_client_identifier();
    printf("client id: %s\n", client_id);
    if (folders_size > 0) monitor_folder(folders, folders_size, client_id, port, host);

    // Free allocated memory
    for (i = 0; i < folders_size; i++) {
        free(folders[i]);
    }
    free(folders);
    free(client_id);

    printf("Finished\n");
    return 0;
}

/**
 * This method allows to get a unique identifier for each client. This identifier is read from the /etc/machine-id file,
 * containing a unique identifier generated during the installation or the boot process if it's not present.
 *
 * @return - the unique identifier of the machine where the client is running on
 */
char* get_client_identifier() {
    // /etc/machine-id contains a 32 characters identifier
    FILE* f = fopen("/etc/machine-id", "r");
    char* uuid = (char*) malloc(32 + 1); // add 1 for null byte
    fgets(uuid, 32 + 1, f);
    fclose(f);
    return uuid;
}

/**
 * This method allows to create the client ssl context
 *
 * @return - the global ssl context of this client
 */
SSL_CTX* create_context() {
    const SSL_METHOD* method;
    SSL_CTX* ctx;

    method = TLS_client_method();

    ctx = SSL_CTX_new(method);
    if (!ctx) {
        perror("Unable to create SSL context");
        exit(EXIT_FAILURE);
    }

    return ctx;
}

/*
 * Allows to specify if we want to continue to monitor folders or not
 */
static volatile sig_atomic_t watch = 1;

/**
 * This method allows to intercept sigint signal to stop watching folders properly
 */
void int_modifier() {
    watch = 0;
}

/**
 * This method allows to monitor for newly created files in a list of specific folders
 *
 * @param folders - list of the folders to monitor
 * @param folders_size - length of the folders list
 * @param client_id - identifier of this client
 */
void monitor_folder(char** folders, int folders_size, char* client_id, int port, char* host) {
    int i;
    int length;
    int file_descriptor;
    int* watch_descriptor = NULL;
    char buffer[BUF_LEN];

    // initialize inotify
    file_descriptor = inotify_init();
    if (file_descriptor < 0) {
        perror("Inotify init failed");
        exit(EXIT_FAILURE);
    }

    watch_descriptor = (int*) malloc(folders_size * sizeof(int));
    // number of inotify_add_watch calls are limited by value in /proc/sys/fs/inotify/max_user_watches
    for (i = 0; i < folders_size; i++)
        watch_descriptor[i] = inotify_add_watch(file_descriptor, folders[i] ,IN_CREATE);

    // handle sigint signals to stop read() and exit program more smoothly
    struct sigaction int_handler = {.sa_handler=int_modifier};
    sigaction(SIGINT,&int_handler,0);

    // while ctrl+c is not pressed, continue to watch for file creation events
    while (watch) {
        length = (int) read(file_descriptor, buffer, BUF_LEN);

        i = 0;
        while (i < length) {
            struct inotify_event *event = (struct inotify_event *) &buffer[i];
            if (event->len) {
                if (event->mask & IN_CREATE) {
                    // use fork system call to create a child process in order to not block next received events
                    if (fork() == 0) {
                        // We sleep before opening and transferring file, in order to let system write large file content
                        // in the monitored folder. There is certainly a less ugly way to wait for file to be completely
                        // written, but I didn't have enough time to digg more. I tried to use the IN_CLOSE_WRITE event,
                        // however it was also triggered when file was modified, but we want to get new created files only.
                        sleep(1);
                        transfer_file(event->name, folders[event->wd - 1], client_id, port, host);
                        exit(EXIT_SUCCESS);
                    }
                }
            }
            i += (int) (EVENT_SIZE + event->len);
        }
    }

    // rm watchers and free memory
    for (i = 0; i < folders_size; i++)
        (void) inotify_rm_watch(file_descriptor, watch_descriptor[i]);
    (void) close(file_descriptor);

    free(watch_descriptor);
}

/**
 * This method allows to create a client socket and connect to server with it's host address and it's listening port
 *
 * @param port - the server port to which we want to connect
 * @param host - the server host address to which we want to connect
 * @return - the client socket
 */
int create_socket(int port, char* host) {
    int sock;
    int conn;

    // create socket
    sock = socket(AF_INET,SOCK_STREAM,0);
    if (sock < 0) {
        perror("Unable to create socket");
        exit(EXIT_FAILURE);
    }

    // initialize server socket struct
    struct sockaddr_in server;
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = inet_addr(host);
    server.sin_port = htons(port);

    // connect to server
    conn = connect(sock,(const struct sockaddr *)&server,(socklen_t) sizeof(server));
    if (conn < 0) {
        perror("Connection to server failed");
        exit(EXIT_FAILURE);
    }

    printf("Connected to server");

    return sock;
}

/**
 * This method allows to build the file path that we want to send to the server.
 *
 * @param file_name - file name of the file to transmit
 * @param folder_path - the folder where the file to send is stored
 * @return - the complete path of the file to send
 */
char* get_file_path(char* file_name, char* folder_path) {
    char* file_path = NULL;
    int malloc_size;
    uint16_t is_path_good;

    printf("\nThe file %s has been created in %s\n", file_name, folder_path);

    // build the filepath of file to transfer with path + name
    malloc_size = (int) (strlen(file_name) + strlen(folder_path) + 1); // add 1 for null byte
    is_path_good = folder_path[strlen(folder_path) - 1] == '/';
    if (!is_path_good) malloc_size++; // if path does not end with a /, increase malloc_size by 1 char
    file_path = (char*) malloc(malloc_size);
    strcpy(file_path, folder_path);
    if (!is_path_good) strcat(file_path, "/"); // if path does not end with a /, concatenate it
    strcat(file_path, file_name);

    return file_path;
}

/**
 * This method allows to compute the sha256 checksum of a file based on it's content
 *
 * @param path - the path of the file from which we want the checksum
 * @param output - the generated sha256 checksum
 */
void sha256sum(char* path, char output[65]) {
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256_CTX sha256;
    char buffer[1024];
    int bytes_read;
    int i;

    // open file to read content from, in order to generate its checksum
    FILE* f = fopen(path, "rb");
    if (f == NULL) {
        perror("file open for sha256sum failed");
        exit(EXIT_FAILURE);
    }

    // initialize sha256 hash
    SHA256_Init(&sha256);

    // update sha256 hash with read content from opened file
    while ((bytes_read = (int) fread(buffer, 1, sizeof(buffer), f)) > 0) {
        SHA256_Update(&sha256, buffer, bytes_read);
    }
    fclose(f);

    // finalize sha256 hash
    SHA256_Final(hash, &sha256);

    // convert unsigned char to char string
    for (i = 0; i < SHA256_DIGEST_LENGTH; i++) {
        sprintf(output + (i * 2), "%02x", hash[i]);
    }
    output[64] = 0;
}

/**
 * This method allows to share all the needed information to share to a server the newly created file in a monitored
 * folder. These information include the file name, the file length, the client id, the sha256 checksum of the original
 * file and the file itself.
 *
 * @param file_name - name of the newly created file
 * @param folder_path - path of the folder where the file has been created
 * @param client_id - the unique client id of this client
 * @param port - the server port to which we want to send the file
 * @param host - the server host address to which we want to send the file
 */
void transfer_file(char* file_name, char* folder_path, char* client_id, int port, char* host) {
    int sock;
    SSL_CTX* ctx;
    SSL* ssl;
    char* file_path;
    uint16_t filename_length;
    uint16_t client_id_length;
    struct stat sb;
    int file_length;
    char sha256_checksum[65]; // 64 + 1 for null byte

    // get file path
    file_path = get_file_path(file_name, folder_path);

    // create server socket, ssl context, and initialize ssl connection
    sock = create_socket(port, host);
    ctx = create_context();
    ssl = SSL_new(ctx);
    SSL_set_fd(ssl, sock);
    if (SSL_connect(ssl) <= 0) {
        perror("SSL connection failed");
        exit(EXIT_FAILURE);
    }

    printf(", with %s encryption\n", SSL_get_cipher(ssl));

    // send client id length and client id to server
    client_id_length = (uint16_t) strlen(client_id);
    SSL_write(ssl, &client_id_length, sizeof(client_id_length));
    SSL_write(ssl, client_id, client_id_length + 1); // add 1 for null byte

    // send filename length and filename to server
    filename_length = (uint16_t) strlen(file_name);
    SSL_write(ssl, &filename_length, sizeof(filename_length));
    SSL_write(ssl, file_name, filename_length + 1); // add 1 for null byte

    // get file length and send it to server
    stat(file_path, &sb);
    file_length = (int) sb.st_size;
    SSL_write(ssl, &file_length, sizeof(file_length));

    // generate sha256 checksum of file and send it to server
    sha256sum(file_path, sha256_checksum);
    SSL_write(ssl, sha256_checksum, sizeof(sha256_checksum));

    printf("File to send is %s, with a length of %d byte(s)\nsha256sum: %s\n", file_path, file_length, sha256_checksum);

    // send file content to server
    FILE* f = fopen(file_path, "rb");
    if (f == NULL) perror("Can't open file");
    // set the send buffer
    char data[CHUNK_SIZE];
    memset(data, 0x00, sizeof(data));
    // read file content and send it while content has been read
    while (fread(data, 1, sizeof(data), f) > 0) {
        if (SSL_write(ssl, data, sizeof(data)) < 0) perror("An error occurred while sending file");
        memset(data, 0x00, sizeof(data));
    }
    fclose(f);

    // wait for server response to close connection
    uint8_t response;
    SSL_read(ssl, &response, sizeof(response));
    printf("Server answered, closing connection\n");
    close(sock);

    // shutdown ssl
    SSL_shutdown(ssl);
    SSL_free(ssl);
    SSL_CTX_free(ctx);

    printf("File %s has been sent\n", file_path);
    free(file_path);
}