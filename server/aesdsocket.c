#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <syslog.h>
#include <signal.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#include <pthread.h>
#include <time.h>
#include <sys/queue.h>

#define PORT 9000
#define BACKLOG 10
#define BUFFER_SIZE 1024
#define FILE_PATH "/var/tmp/aesdsocketdata"

int server_socket = -1;
pthread_mutex_t file_mutex = PTHREAD_MUTEX_INITIALIZER;
volatile sig_atomic_t exit_flag = 0;

typedef struct {
    int client_socket;
    struct sockaddr_in client_addr;
} thread_args_t;

typedef struct thread_node {
    pthread_t thread;
    SLIST_ENTRY(thread_node) entries;
} thread_node_t;

SLIST_HEAD(thread_list, thread_node) head = SLIST_HEAD_INITIALIZER(head);

void print_file_to_stdout(const char *file_path) {
    FILE *file = fopen(file_path, "r");
    if (file == NULL) {
        perror("Failed to open file");
        return;
    }

    char buffer[BUFFER_SIZE];
    size_t bytes;

    while ((bytes = fread(buffer, 1, BUFFER_SIZE, file)) > 0) {
        fwrite(buffer, 1, bytes, stdout);
    }

    fclose(file);
}

void run_as_daemon() {
    pid_t pid, sid;

    pid = fork();
    if (pid < 0) {
        exit(EXIT_FAILURE);
    }
    if (pid > 0) {
        exit(EXIT_SUCCESS);
    }

    umask(0);
    sid = setsid();
    if (sid < 0) {
        exit(EXIT_FAILURE);
    }
    if ((chdir("/")) < 0) {
        exit(EXIT_FAILURE);
    }

    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);

    int log_fd = open("/dev/null", O_RDWR);
    if (log_fd == -1) {
        exit(EXIT_FAILURE);
    }

    dup2(log_fd, STDIN_FILENO);
    dup2(log_fd, STDOUT_FILENO);
    dup2(log_fd, STDERR_FILENO);
}

void handle_signal(int signal) {
    syslog(LOG_INFO, "Caught signal %d, exiting", signal);
    exit_flag = 1;
    close(server_socket);
}

void setup_signal_handlers() {
    struct sigaction sa;
    sa.sa_handler = handle_signal;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    if (sigaction(SIGINT, &sa, NULL) == -1) {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }
    if (sigaction(SIGTERM, &sa, NULL) == -1) {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }
}

void* handle_client(void* arg) {
    thread_args_t* args = (thread_args_t*)arg;
    int client_socket = args->client_socket;
    struct sockaddr_in client_addr = args->client_addr;
    free(arg);

    char buffer[BUFFER_SIZE];
    ssize_t bytes_received;

    syslog(LOG_INFO, "Accepted connection from %s and socket_id:%d", inet_ntoa(client_addr.sin_addr), client_socket);
    
    while ((bytes_received = recv(client_socket, buffer, BUFFER_SIZE - 1, 0)) > 0) {
        buffer[bytes_received] = '\0';

        pthread_mutex_lock(&file_mutex);
        FILE* file = fopen(FILE_PATH, "a+");
        if (file == NULL) {
            syslog(LOG_ERR, "Failed to open file: %s", strerror(errno));
            pthread_mutex_unlock(&file_mutex);
            close(client_socket);
            return NULL;
        }

        fprintf(file, "%s", buffer);
        fflush(file);

        if (strchr(buffer, '\n') != NULL) {
            fseek(file, 0, SEEK_SET);
            while (fgets(buffer, BUFFER_SIZE, file) != NULL) {
                send(client_socket, buffer, strlen(buffer), 0);
            }
        }

        fclose(file);
        pthread_mutex_unlock(&file_mutex);
    }

    if (bytes_received == -1) {
        syslog(LOG_ERR, "Failed to receive data: %s", strerror(errno));
    }

    syslog(LOG_INFO, "Closed connection from %s and socket_id:%d", inet_ntoa(client_addr.sin_addr), client_socket);

    close(client_socket);
    return NULL;
}

void* append_timestamps() {
    while (!exit_flag) {
        sleep(10);

        time_t now = time(NULL);
        struct tm* tm_info = localtime(&now);
        char timestamp[64];
        strftime(timestamp, sizeof(timestamp), "timestamp:%a, %d %b %Y %H:%M:%S %z\n", tm_info);

        pthread_mutex_lock(&file_mutex);
        FILE* file = fopen(FILE_PATH, "a");
        if (file != NULL) {
            fprintf(file, "%s", timestamp);
            fflush(file);
            fclose(file);
        }
        pthread_mutex_unlock(&file_mutex);
    }
    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc > 1 && strcmp(argv[1], "-d") == 0) {
        run_as_daemon();
    }

    syslog(LOG_ERR, "------------ SERVER STARTING -----------------");
    remove(FILE_PATH);

    struct sockaddr_in server_addr, client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    pthread_t timestamp_thread;

    openlog("aesdsocket", LOG_PID, LOG_USER);
    setup_signal_handlers();

    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1) {
        syslog(LOG_ERR, "Failed to create socket: %s", strerror(errno));
        return -1;
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        syslog(LOG_ERR, "Failed to bind socket: %s", strerror(errno));
        close(server_socket);
        return -1;
    }

    if (listen(server_socket, BACKLOG) == -1) {
        syslog(LOG_ERR, "Failed to listen on socket: %s", strerror(errno));
        close(server_socket);
        return -1;
    }

    if (pthread_create(&timestamp_thread, NULL, append_timestamps, NULL) != 0) {
        syslog(LOG_ERR, "Failed to create timestamp thread: %s", strerror(errno));
        close(server_socket);
        return -1;
    }

    while (!exit_flag) {
        int client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &client_addr_len);
        if (client_socket == -1) {
            if (exit_flag) break;
            syslog(LOG_ERR, "Failed to accept connection: %s", strerror(errno));
            continue;
        }

        thread_args_t* args = malloc(sizeof(thread_args_t));
        if (args == NULL) {
            syslog(LOG_ERR, "Failed to allocate memory for thread args: %s", strerror(errno));
            close(client_socket);
            continue;
        }
        args->client_socket = client_socket;
        args->client_addr = client_addr;

        thread_node_t* new_node = malloc(sizeof(thread_node_t));
        if (new_node == NULL) {
            syslog(LOG_ERR, "Failed to allocate memory for thread node: %s", strerror(errno));
            close(client_socket);
            free(args);
            continue;
        }

        if (pthread_create(&new_node->thread, NULL, handle_client, args) != 0) {
            syslog(LOG_ERR, "Failed to create client thread: %s", strerror(errno));
            close(client_socket);
            free(args);
            free(new_node);
            continue;
        }

        SLIST_INSERT_HEAD(&head, new_node, entries);

    }

    pthread_join(timestamp_thread, NULL);

    thread_node_t* node;
    while (!SLIST_EMPTY(&head)) {
        node = SLIST_FIRST(&head);
        pthread_join(node->thread, NULL);
        SLIST_REMOVE_HEAD(&head, entries);
        free(node);
    }

    close(server_socket);
    closelog();
    return 0;
}