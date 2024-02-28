#include "client.h"

/**
 * This client allows to monitor a list of folder for newly created files. The new files are then sent to the server
 * through a connection secured by ssl, allowing to ensure confidentiality of data.
 *
 * @param argc arguments count
 * @param argv arguments value
 * @return exit code
 */
int main(int argc, char **argv) {
    int port;
    char* host = NULL;
    int i;
    int opt;
    int folders_size;
    char** folders = NULL;
    int argv_index;

    opterr = 0;

    // handle provided options to the program using getopt
    while ((opt = getopt(argc, argv, "p:h:")) != -1) {
        switch (opt) {
            case 'p':
                port = atoi(optarg);
                break;
            case 'h':
                host = optarg;
                break;
            case '?':
                // handle required and unknown options
                if (optopt == 'p' || optopt == 'h') {
                    fprintf(stderr, "Option -%c requires an argument.\n", optopt);
                } else if (isprint(optopt)) {
                    fprintf(stderr, "Unknown option `-%c'.\n", optopt);
                } else {
                    fprintf(stderr, "Unknown option character `\\x%x'.\n", optopt);
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
        abort();
    }

    // retrieve the list of folders to monitor
    for (i = 0; i < folders_size; i++) {
        // argv index is the index of the next folder path to process
        // argv_index = last processed arg by getopt + i
        argv_index = optind + i;
        folders[i] = (char*) malloc(strlen(argv[argv_index]) * sizeof(char));
        if (folders[i] == NULL) {
            perror("folders[i] malloc");
            abort();
        }
        strcpy(folders[i], argv[argv_index]);
    }

    monitor_folder(folders, folders_size);

    // Free allocated memory
    for (i = 0; i < folders_size; i++) {
        free(folders[i]);
    }
    free(folders);

    return 0;
}

/*
 * Allows to specify if we want to continue to monitor folders or not
 */
static volatile sig_atomic_t watch = 1;

/**
 * Allows to intercept sigint signal to stop watching folders properly
 *
 * @param _
 */
void sigint_interceptor(int _) {
    watch = 0;
    printf("\nSIGINT intercepted. Program will exit on next received event.\n");
}

/**
 * Allows to monitor for newly created files in a list of specific folders
 *
 * @param folders - list of the folders to monitor
 * @param folders_size - length of the folders list
 */
void monitor_folder(char** folders, int folders_size) {
    int length, i;
    int file_descriptor;
    int* watch_descriptor = NULL;
    char buffer[BUF_LEN];

    // handle sigint signals
    signal(SIGINT, sigint_interceptor);

    file_descriptor = inotify_init();
    if (file_descriptor < 0) {
        perror("inotify_init");
        abort();
    }

    watch_descriptor = (int*) malloc(folders_size * sizeof(int));
    // number of inotify_add_watch calls are limited by value in /proc/sys/fs/inotify/max_user_watches
    for (i = 0; i < folders_size; i++)
        watch_descriptor[i] = inotify_add_watch(file_descriptor, folders[i] ,IN_CREATE);

    while (watch) {
        // todo : maybe we can set read() in a thread, so when a signal is intercepted we can terminate the thread properly
        length = (int) read(file_descriptor, buffer, BUF_LEN);

        if (length < 0) perror("read");

        i = 0;
        while (i < length) {
            struct inotify_event *event = (struct inotify_event *) &buffer[i];
            if (event->len) {
                if (event->mask & IN_CREATE) {
                    printf("The file %s was created.\n", event->name);
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