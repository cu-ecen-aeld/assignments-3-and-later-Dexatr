#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <syslog.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/stat.h>

#define PORT 9000
#define BACKLOG 10
#define BUFFER_SIZE 1024
#define FILENAME "/var/tmp/aesdsocketdata"

int server_fd = -1;
int client_fd = -1;
FILE *file_ptr = NULL;

void handle_sigint(int sig) {
    syslog(LOG_INFO, "Caught signal, exiting");

    if (client_fd >= 0) {
        close(client_fd);
    }

    if (server_fd >= 0) {
        close(server_fd);
    }

    if (file_ptr) {
        fclose(file_ptr);
        remove(FILENAME);
    }

    syslog(LOG_INFO, "Cleanup complete, exiting now.");
    closelog();
    exit(EXIT_SUCCESS);
}

void setup_signal_handler() {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_sigint;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
}

void daemonize() {
    pid_t pid = fork();

    if (pid < 0) {
        syslog(LOG_ERR, "Fork failed: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }

    if (pid > 0) {
        exit(EXIT_SUCCESS);
    }

    if (setsid() < 0) {
        syslog(LOG_ERR, "Failed to create new session: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }

    pid = fork();

    if (pid < 0) {
        syslog(LOG_ERR, "Second fork failed: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }

    if (pid > 0) {
        exit(EXIT_SUCCESS);
    }

    umask(0);
    chdir("/");

    for (int i = sysconf(_SC_OPEN_MAX); i >= 0; i--) {
        close(i);
    }

    openlog("aesdsocket", LOG_PID, LOG_DAEMON);
    syslog(LOG_INFO, "Daemon started successfully");
}

int main(int argc, char *argv[]) {
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    char buffer[BUFFER_SIZE];
    ssize_t num_bytes;
    char client_ip[INET_ADDRSTRLEN];

    openlog("aesdsocket", LOG_PID | LOG_CONS, LOG_USER);
    syslog(LOG_INFO, "aesdsocket starting");

    if (argc == 2 && strcmp(argv[1], "-d") == 0) {
        syslog(LOG_INFO, "Daemon mode enabled");
        daemonize();
    }

    setup_signal_handler();

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        syslog(LOG_ERR, "Error creating socket: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(PORT);

    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        syslog(LOG_ERR, "Error setting socket options: %s", strerror(errno));
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        syslog(LOG_ERR, "Error binding socket: %s", strerror(errno));
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, BACKLOG) < 0) {
        syslog(LOG_ERR, "Error listening on socket: %s", strerror(errno));
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    syslog(LOG_INFO, "Server listening on port %d", PORT);

    while (1) {
        client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_addr_len);
        if (client_fd < 0) {
            syslog(LOG_ERR, "Error accepting connection: %s", strerror(errno));
            continue; // Do not exit, try to accept new connections
        }

        inet_ntop(AF_INET, &(client_addr.sin_addr), client_ip, INET_ADDRSTRLEN);
        syslog(LOG_INFO, "Accepted connection from %s", client_ip);

        file_ptr = fopen(FILENAME, "a+");
        if (!file_ptr) {
            syslog(LOG_ERR, "Error opening file: %s", strerror(errno));
            close(client_fd);
            continue; // Move to the next client connection
        }

        while ((num_bytes = recv(client_fd, buffer, BUFFER_SIZE - 1, 0)) > 0) {
            buffer[num_bytes] = '\0';
            fprintf(file_ptr, "%s", buffer);
            fflush(file_ptr);

            if (strchr(buffer, '\n')) {
                fseek(file_ptr, 0, SEEK_SET);
                while (fgets(buffer, BUFFER_SIZE, file_ptr)) {
                    send(client_fd, buffer, strlen(buffer), 0);
                }
            }
        }

        if (num_bytes < 0) {
            syslog(LOG_ERR, "Error receiving data: %s", strerror(errno));
        }

        syslog(LOG_INFO, "Closed connection from %s", client_ip);

        close(client_fd);
        fclose(file_ptr);
        file_ptr = NULL; // Reset the file pointer
    }

    close(server_fd);
    syslog(LOG_INFO, "Server shutting down");
    closelog();

    return 0;
}
