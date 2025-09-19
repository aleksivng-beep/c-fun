#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdatomic.h>
#include <pthread.h>

#define PORT 8080

static int server_socket = -1;
static _Atomic int running = 0;
static pthread_t server_thread;
static _Atomic int server_thread_started = 0;

static void handle_client(int client_socket) {
    char buffer[2048];
    ssize_t n = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
    if (n < 0) {
        perror("recv failed");
        close(client_socket);
        return;
    }
    buffer[n] = '\0';
    printf("Request:\n%.*s\n", (int)n, buffer);

    const char body[] = "Hello, world!";
    char header[256];
    int header_len = snprintf(header, sizeof(header),
                              "HTTP/1.1 200 OK\r\n"
                              "Content-Type: text/plain\r\n"
                              "Connection: close\r\n"
                              "Content-Length: %zu\r\n\r\n",
                              strlen(body));

    if (send(client_socket, header, header_len, 0) < 0) {
        perror("send header failed");
    } else if (send(client_socket, body, sizeof(body) - 1, 0) < 0) {
        perror("send body failed");
    }

    close(client_socket);
}

static void *server_main(void *arg) {
    (void)arg;

    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1) {
        perror("Socket creation failed");
        goto out;
    }

    int opt = 1;
    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt(SO_REUSEADDR) failed");
    }
#ifdef SO_REUSEPORT
    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt)) < 0) {
        perror("setsockopt(SO_REUSEPORT) failed");
    }
#endif

    struct sockaddr_in server_address;
    memset(&server_address, 0, sizeof(server_address));
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = INADDR_ANY;
    server_address.sin_port = htons(PORT);

    if (bind(server_socket, (struct sockaddr *)&server_address, sizeof(server_address)) < 0) {
        perror("Bind failed");
        goto out_close;
    }

    if (listen(server_socket, 128) < 0) {
        perror("Listen failed");
        goto out_close;
    }

    printf("Server is running on port %d\n", PORT);

    while (atomic_load(&running)) {
        int client_socket = accept(server_socket, NULL, NULL);
        if (!atomic_load(&running)) {
            break;
        }
        if (client_socket < 0) {
            if (errno == EINTR) {
                continue; // interrupted by signal/shutdown
            }
            perror("Accept failed");
            continue;
        }
        handle_client(client_socket);
    }

out_close:
    if (server_socket != -1) {
        close(server_socket);
        server_socket = -1;
    }
    printf("Server stopped.\n");

out:
    atomic_store(&server_thread_started, 0);
    return NULL;
}

__attribute__((visibility("default"))) void start_server() {
    if (atomic_load(&server_thread_started)) {
        printf("Server already running.\n");
        return;
    }
    atomic_store(&running, 1);
    printf("Starting server...\n");
    if (pthread_create(&server_thread, NULL, server_main, NULL) != 0) {
        perror("pthread_create failed");
        atomic_store(&running, 0);
        return;
    }
    atomic_store(&server_thread_started, 1);
}

__attribute__((visibility("default"))) void stop_server() {
    if (!atomic_load(&server_thread_started)) {
        return;
    }
    atomic_store(&running, 0);
    if (server_socket != -1) {
        shutdown(server_socket, SHUT_RDWR);
    }
    // Wait for the server thread to finish
    pthread_join(server_thread, NULL);
}
