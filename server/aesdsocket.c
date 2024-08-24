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

#define PORT 9000
#define BACKLOG 10
#define BUFFER_SIZE 1024
#define FILE_PATH "/var/tmp/aesdsocketdata"

int server_socket = -1;
int client_socket = -1;
FILE *file = NULL;

void run_as_daemon() {
    pid_t pid, sid;

    // Fork the parent process
    pid = fork();

    // If fork() returns a negative value, creation of child process was unsuccessful
    if (pid < 0) {
        exit(EXIT_FAILURE);
    }

    // If fork() returns a positive value, we are in the parent process
    if (pid > 0) {
        exit(EXIT_SUCCESS);
    }

    // Change the file mode mask
    umask(0);

    // Create a new SID for the child process
    sid = setsid();
    if (sid < 0) {
        exit(EXIT_FAILURE);
    }

    // Change the current working directory
    if ((chdir("/")) < 0) {
        exit(EXIT_FAILURE);
    }

    // Close out the standard file descriptors
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);

    // Open log file
    int log_fd = open("/dev/null", O_RDWR);
    if (log_fd == -1) {
        exit(EXIT_FAILURE);
    }

    // Redirect standard file descriptors to /dev/null
    dup2(log_fd, STDIN_FILENO);
    dup2(log_fd, STDOUT_FILENO);
    dup2(log_fd, STDERR_FILENO);
}

void handle_signal(int signal) {
    syslog(LOG_INFO, "Caught signal %d, exiting", signal);
    if (client_socket != -1) {
        close(client_socket);
    }
    if (server_socket != -1) {
        close(server_socket);
    }
    if (file != NULL) {
        fclose(file);
        remove(FILE_PATH);
    }
    closelog();
    exit(0);
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

int main(int argc, char *argv[]) {

    // Run as daemon
    if (argc > 1 && strcmp(argv[1], "-d") == 0) {
        run_as_daemon();
    }

    // Remove file if exist
    remove(FILE_PATH);

    struct sockaddr_in server_addr, client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    char buffer[BUFFER_SIZE];
    ssize_t bytes_received;

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

    while (1) {
        client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &client_addr_len);
        if (client_socket == -1) {
            syslog(LOG_ERR, "Failed to accept connection: %s", strerror(errno));
            continue;
        }

        syslog(LOG_INFO, "Accepted connection from %s", inet_ntoa(client_addr.sin_addr));

        file = fopen(FILE_PATH, "a+");
        if (file == NULL) {
            syslog(LOG_ERR, "Failed to open file: %s", strerror(errno));
            close(client_socket);
            continue;
        }

        while ((bytes_received = recv(client_socket, buffer, BUFFER_SIZE - 1, 0)) > 0) {
            buffer[bytes_received] = '\0';
            fprintf(file, "%s", buffer);
            fflush(file);

            if (strchr(buffer, '\n') != NULL) {
                fseek(file, 0, SEEK_SET);
                while (fgets(buffer, BUFFER_SIZE, file) != NULL) {
                    send(client_socket, buffer, strlen(buffer), 0);
                }
                fseek(file, 0, SEEK_END);
            }
        }

        if (bytes_received == -1) {
            syslog(LOG_ERR, "Failed to receive data: %s", strerror(errno));
        }

        syslog(LOG_INFO, "Closed connection from %s", inet_ntoa(client_addr.sin_addr));
        fclose(file);
        file = NULL;
        close(client_socket);
        client_socket = -1;
    }

    return 0;
}