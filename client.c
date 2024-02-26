#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/inotify.h>

#define EVENT_SIZE  (sizeof(struct inotify_event))
#define BUF_LEN     (1024 * (EVENT_SIZE + 16))

/**
 * This client allows to monitor a list of folder for newly created files. The new files are then sent to the server
 * through a connection secured by ssl, allowing to ensure confidentiality of data.
 *
 * @param argc arguments count
 * @param argv arguments value
 * @return exit code
 */
int main(int argc, char **argv) {
    int port = 0;
    char *host = NULL;
    int index;
    int c;

    opterr = 0;

    while ((c = getopt(argc, argv, "p:h:")) != -1) {
        switch (c) {
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

    // allows to get the list of folders to monitor
    for (index = optind; index < argc; index++) {
        printf("Non-option argument %s\n", argv[index]);
    }

    return 0;
}

void monitor_folder() {
    int length, i = 0;
    int file_descriptor;
    int watch_descriptor;
    char buffer[BUF_LEN];

    file_descriptor = inotify_init();

    if (file_descriptor < 0) perror("inotify_init");

    watch_descriptor = inotify_add_watch(file_descriptor, "../../monitored-folder-1",IN_CREATE);
    length = read(file_descriptor, buffer, BUF_LEN);

    if (length < 0) perror("read");

    while (i < length) {
        struct inotify_event *event = (struct inotify_event *) &buffer[i];
        if (event->len) {
            if (event->mask & IN_CREATE) {
                printf("The file %s was created.\n", event->name);
            }
        }
        i += EVENT_SIZE + event->len;
    }

    (void) inotify_rm_watch(file_descriptor, watch_descriptor);
    (void) close(file_descriptor);
}