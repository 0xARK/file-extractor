#include "client.h"

/**
 * This client allows to monitor a list of folder for newly created files. The new files are then sent to the server
 * through a connection secured by ssl, allowing to ensure confidentiality of data.
 *
 * @param argc arguments count
 * @param argv arguments value
 * @return exit code
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
     * The following sections allows to process the folders paths provided in parameters.
     * These paths are stored in a dynamic array in order to not limit the user to a specific number of folder to monitor.
     * The length of the path string is also dynamic, to not restrict user too.
     */

    // folders size = total arg count - last processed arg by getopt
    folders_size = argc - optind;
    folders = (char**) malloc(folders_size * sizeof(char*));
    if (folders == NULL) {
        perror("folders malloc");
        exit(1);
    }

    // retrieve the list of folders to monitor
    for (i = 0; i < folders_size; i++) {
        // argv index is the index of the next folder path to process
        // argv_index = last processed arg by getopt + i
        argv_index = optind + i;
        folders[i] = (char*) malloc(strlen(argv[argv_index]));
        if (folders[i] == NULL) {
            perror("folders[i] malloc");
            exit(1);
        }
        strcpy(folders[i], argv[argv_index]);
    }

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
 * Allows to get a unique identifier for each client
 *
 * @return - the unique identifier of the machine where the client is running on
 */
char* get_client_identifier() {
    // /etc/machine-id contains a 32 characters identifier
    FILE* f = fopen("/etc/machine-id", "r");
    // add 1 to store '\0' byte
    char* uuid = (char*) malloc(32 + 1);
    fgets(uuid, 32 + 1, f);
    fclose(f);
    return uuid;
}

/*
 * Allows to specify if we want to continue to monitor folders or not
 */
static volatile sig_atomic_t watch = 1;

/**
 * Allows to intercept sigint signal to stop watching folders properly
 */
void sigint_interceptor() {
    watch = 0;
}

/**
 * Allows to monitor for newly created files in a list of specific folders
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

    file_descriptor = inotify_init();
    if (file_descriptor < 0) {
        perror("inotify_init");
        exit(1);
    }

    watch_descriptor = (int*) malloc(folders_size * sizeof(int));
    // number of inotify_add_watch calls are limited by value in /proc/sys/fs/inotify/max_user_watches
    for (i = 0; i < folders_size; i++)
        watch_descriptor[i] = inotify_add_watch(file_descriptor, folders[i] ,IN_CREATE);

    // handle sigint signals to stop read() and exit program more smoothly
    struct sigaction int_handler = {.sa_handler=sigint_interceptor};
    sigaction(SIGINT,&int_handler,0);

    while (watch) {
        length = (int) read(file_descriptor, buffer, BUF_LEN);

        i = 0;
        while (i < length) {
            struct inotify_event *event = (struct inotify_event *) &buffer[i];
            if (event->len) {
                if (event->mask & IN_CREATE) {
                    transfer_file(event->name, folders[event->wd -1], client_id, port, host);
                }
            }
            i += EVENT_SIZE + event->len;
        }
    }

    for (i = 0; i < folders_size; i++)
        (void) inotify_rm_watch(file_descriptor, watch_descriptor[i]);
    (void) close(file_descriptor);

    free(watch_descriptor);
}

void transfer_file(char* filename, char* filepath, char* client_id, int port, char* host) {
    char* filename_with_path = NULL;
    int malloc_size;
    uint16_t is_path_good;
    int sock;
    int conn;
    uint16_t filename_length;
    uint16_t client_id_length;

    printf("The file %s was created in %s.\n", filename, filepath);

    // build the filepath of file to transfer with path + name
    malloc_size = (int) (strlen(filename) + strlen(filepath) + 1); // add 1 for null byte
    is_path_good = filepath[strlen(filepath) - 1] == '/';
    if (!is_path_good) malloc_size++; // if path does not end with a /, increase malloc_size by 1 char
    filename_with_path = (char*) malloc(malloc_size);
    strcpy(filename_with_path, filepath);
    if (!is_path_good) strcat(filename_with_path, "/"); // if path does not end with a /, concatenate it
    strcat(filename_with_path, filename);

    // create socket
    sock = socket(AF_INET,SOCK_STREAM,0);
    if (sock < 0) {
        perror("sock");
        exit(1);
    }

    // initialize server socket struct
    struct sockaddr_in server;
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = inet_addr(host);
    server.sin_port = htons(port);

    conn = connect(sock,(const struct sockaddr *)&server,(socklen_t) sizeof(server));
    if (conn < 0) {
        perror("connection");
        exit(1);
    }

    printf("Connected to server.\n");

    // send client id length and client id to server
    client_id_length = (uint16_t) strlen(client_id);
    send(sock, &client_id_length, sizeof(client_id_length), 0);
    send(sock, client_id, client_id_length + 1, 0); // add 1 for null byte

    // send filename length and filename to server
    filename_length = (uint16_t) strlen(filename);
    send(sock, &filename_length, sizeof(filename_length), 0);
    send(sock, filename, filename_length + 1, 0); // add 1 for null byte

    // get file length
    struct stat sb;
    stat(filename_with_path, &sb);
    off_t file_length = sb.st_size;

    // send file length to server
    int length_to_send = (int) file_length;
    send(sock,&length_to_send,sizeof(length_to_send), 0);

    printf("File to send is %s, with a length of %ld\n", filename_with_path, file_length - 1); // minus 1 for null byte

    // send file content to server
    FILE* f = fopen(filename_with_path, "r");
    while(!feof(f)) {
        char c = (char) fgetc(f);
        send(sock,&c,sizeof(c),0);
    }

    fclose(f);
    close(sock);

    // free allocated memory
    free(filename_with_path);
}