#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

#define PORT 4221
#define BUFFER_SIZE 16384

char files_directory[BUFFER_SIZE] = ".";

void send_response(int client_fd, const char *status, const char *content_type,
                   const char *body, size_t body_length);
void *handle_client(void *arg);void handle_files_endpoint(int client_fd, const char *filename, int head_only);

static bool is_safe_filename(const char *name) {
  if (!name || name[0] == '\0') return false;
  if (name[0] == '/' || strstr(name, "..") != NULL || strchr(name, '\\') != NULL) {
    return false;
  }
  return true;
}

int main(int argc, char *argv[]) {

  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--directory") == 0 && i + 1 < argc) {
      strncpy(files_directory, argv[i + 1], BUFFER_SIZE - 1);
      files_directory[BUFFER_SIZE - 1] = '\0';
    }
  }

  // Disable output buffering
  setbuf(stdout, NULL);
  setbuf(stderr, NULL);

  int server_fd, client_addr_len;
  struct sockaddr_in client_addr;

  server_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (server_fd == -1) {
    printf("Socket creation failed: %s...\n", strerror(errno));
    return 1;
  }

  // // Since the tester restarts your program quite often, setting SO_REUSEADDR
  // // ensures that we don't run into 'Address already in use' errors
  int reuse = 1;
  if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) <
      0) {
    printf("SO_REUSEADDR failed: %s \n", strerror(errno));
    return 1;
  }

  struct sockaddr_in serv_addr = {
      .sin_family = AF_INET,
      .sin_port = htons(PORT),
      .sin_addr = {htonl(INADDR_ANY)},
  };

  if (bind(server_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) != 0) {
    printf("Bind failed: %s \n", strerror(errno));
    return 1;
  }

  int connection_backlog = 5;
  if (listen(server_fd, connection_backlog) != 0) {
    printf("Listen failed: %s \n", strerror(errno));
    return 1;
  }

  printf("Waiting for a client to connect...\n");
  client_addr_len = sizeof(client_addr);

  while (1) {
    int *client_fd = malloc(sizeof(int));
    if (!client_fd) {
      printf("Memory allocation failed: %s \n", strerror(errno));
      continue;
    }

    *client_fd = accept(server_fd, (struct sockaddr *)&client_addr,
                        (socklen_t *)&client_addr_len);

    if (*client_fd < 0) {
      printf("Connection failed: %s \n", strerror(errno));
      free(client_fd);
      continue;
    }
    printf("Client connected\n");

    pthread_t thread_id;
    if (pthread_create(&thread_id, NULL, handle_client, client_fd) != 0) {
      printf("Thread creation failed: %s \n", strerror(errno));
      close(*client_fd);
      free(client_fd);
      continue;
    }

    pthread_detach(thread_id);
  }

  close(server_fd);
  return 0;
}

void *handle_client(void *arg) {
  int client_fd = (*(int *)arg);
  free(arg);

  char buffer[BUFFER_SIZE + 1];
  int total = 0;
  int n = 0;

  // Read until headers are complete (\r\n\r\n) or buffer is full
  while (total < BUFFER_SIZE) {
    n = read(client_fd, buffer + total, BUFFER_SIZE - total);
    if (n < 0) {
      printf("Read failed: %s \n", strerror(errno));
      close(client_fd);
      return NULL;
    }
    if (n == 0) {
      break;
    }
    total += n;
    buffer[total] = '\0';
    if (strstr(buffer, "\r\n\r\n") != NULL) {
      break;
    }
  }

  if (total == 0) {
    close(client_fd);
    return NULL;
  }

  printf("Request received:\n%.*s\n", total, buffer);

  char method[16] = {0}, path[256] = {0}, version[16] = {0};
  sscanf(buffer, "%15s %255s %15s", method, path, version);
  printf("Request: %s %s %s\n", method, path, version);

  int is_head = (strcmp(method, "HEAD") == 0);

  if (strcmp(method, "GET") == 0 || is_head) {
    if (strcmp(path, "/") == 0) {
      send_response(client_fd, "200 OK", "text/plain", NULL, 0);
    } else if (strncmp(path, "/echo/", 6) == 0) {
      char *echo_str = path + 6;
      size_t len = strlen(echo_str);
      if (is_head) {
        send_response(client_fd, "200 OK", "text/plain", NULL, len);
      } else {
        send_response(client_fd, "200 OK", "text/plain", echo_str, len);
      }
    } else if (strcmp(path, "/user-agent") == 0) {
      // Find user-agent header
      char *ua_start = strstr(buffer, "User-Agent: ");
      if (ua_start) {
        ua_start += 12;
        char *ua_end = strstr(ua_start, "\r\n");
        if (!ua_end) {
          send_response(client_fd, "400 Bad Request", "text/plain", NULL, 0);
        } else {
          int ua_len = (int)(ua_end - ua_start);
          if (ua_len < 0) ua_len = 0;
          char user_agent[ua_len + 1];
          strncpy(user_agent, ua_start, ua_len);
          user_agent[ua_len] = '\0';

          if (is_head) {
            send_response(client_fd, "200 OK", "text/plain", NULL, (size_t)ua_len);
          } else {
            send_response(client_fd, "200 OK", "text/plain", user_agent, (size_t)ua_len);
          }
        }
      } else {
        send_response(client_fd, "400 Bad Request", "text/plain", NULL, 0);
      }
    } else if (strncmp(path, "/files/", 7) == 0) {
      char *filename = path + 7;
      if (!is_safe_filename(filename)) {
        send_response(client_fd, "400 Bad Request", "text/plain", NULL, 0);
      } else {
        handle_files_endpoint(client_fd, filename, is_head);
      }
    } else {
      send_response(client_fd, "404 Not Found", "text/plain", NULL, 0);
    }
  } else if (strcmp(method, "POST") == 0) {
    if (strncmp(path, "/files/", 7) == 0) {
      char *filename = path + 7;
      if (!is_safe_filename(filename)) {
        send_response(client_fd, "400 Bad Request", "text/plain", NULL, 0);
      } else {
        char *headers_end = strstr(buffer, "\r\n\r\n");
        if (!headers_end) {
          send_response(client_fd, "400 Bad Request", "text/plain", NULL, 0);
        } else {
          char *cl_start = strstr(buffer, "Content-Length: ");
          if (!cl_start) {
            send_response(client_fd, "400 Bad Request", "text/plain", NULL, 0);
          } else {
            cl_start += 16; // length of "Content-Length: "
            long content_length = strtol(cl_start, NULL, 10);
            if (content_length < 0 || content_length > 1073741824L) {
              send_response(client_fd, "400 Bad Request", "text/plain", NULL, 0);
            } else {
              size_t cl = (size_t)content_length;
              size_t header_bytes = (size_t)((headers_end + 4) - buffer);
              size_t already = total > (int)header_bytes ? (size_t)(total - (int)header_bytes) : 0;

              char *body_buf = NULL;
              if (cl > 0) {
                body_buf = (char *)malloc(cl);
                if (!body_buf) {
                  send_response(client_fd, "500 Internal Server Error", "text/plain", NULL, 0);
                  close(client_fd);
                  return NULL;
                }
                size_t to_copy = already > cl ? cl : already;
                if (to_copy > 0) {
                  memcpy(body_buf, buffer + header_bytes, to_copy);
                }
                size_t remaining = cl - to_copy;
                while (remaining > 0) {
                  ssize_t r = read(client_fd, body_buf + to_copy, remaining);
                  if (r < 0) {
                    free(body_buf);
                    send_response(client_fd, "500 Internal Server Error", "text/plain", NULL, 0);
                    close(client_fd);
                    return NULL;
                  }
                  if (r == 0) break;
                  to_copy += (size_t)r;
                  remaining -= (size_t)r;
                }
                if (remaining != 0) {
                  free(body_buf);
                  send_response(client_fd, "400 Bad Request", "text/plain", NULL, 0);
                  close(client_fd);
                  return NULL;
                }
              }

              // Write file
              char file_path[BUFFER_SIZE];
              snprintf(file_path, BUFFER_SIZE, "%s/%s", files_directory, filename);
              FILE *out = fopen(file_path, "wb");
              if (!out) {
                if (body_buf) free(body_buf);
                send_response(client_fd, "500 Internal Server Error", "text/plain", NULL, 0);
              } else {
                if (cl > 0 && body_buf) {
                  size_t written = fwrite(body_buf, 1, cl, out);
                  (void)written;
                }
                fclose(out);
                if (body_buf) free(body_buf);
                send_response(client_fd, "201 Created", "text/plain", NULL, 0);
              }
            }
          }
        }
      }
    } else {
      send_response(client_fd, "404 Not Found", "text/plain", NULL, 0);
    }
  } else {
    send_response(client_fd, "405 Method Not Allowed", "text/plain", NULL, 0);
  }

  close(client_fd);
  return NULL;
}

void send_response(int client_fd, const char *status, const char *content_type,
                   const char *body, size_t body_length) {
  char header[BUFFER_SIZE];
  int header_length = snprintf(header, BUFFER_SIZE,
                               "HTTP/1.1 %s\r\n"
                               "Content-Type: %s\r\n"
                               "Content-Length: %zu\r\n"
                               "Connection: close\r\n"
                               "Server: c-http/0.1\r\n"
                               "\r\n",
                               status, content_type ? content_type : "application/octet-stream", body_length);

  write(client_fd, header, header_length);
  if (body && body_length > 0) {
    write(client_fd, body, body_length);
  }
}

void handle_files_endpoint(int client_fd, const char *filename, int head_only) {
  char file_path[BUFFER_SIZE];
  snprintf(file_path, BUFFER_SIZE, "%s/%s", files_directory, filename);

  FILE *file = fopen(file_path, "rb");
  if (file == NULL) {
    send_response(client_fd, "404 Not Found", "text/plain", NULL, 0);
    return;
  }

  fseek(file, 0, SEEK_END);
  long file_size = ftell(file);
  fseek(file, 0, SEEK_SET);

  if (head_only) {
    fclose(file);
    send_response(client_fd, "200 OK", "application/octet-stream", NULL, (size_t)file_size);
    return;
  }

  char *file_content = malloc((size_t)file_size);
  if (!file_content) {
    fclose(file);
    send_response(client_fd, "500 Internal Server Error", "text/plain", NULL,
                  0);
    return;
  }

  fread(file_content, 1, (size_t)file_size, file);
  fclose(file);

  send_response(client_fd, "200 OK", "application/octet-stream", file_content,
                (size_t)file_size);

  free(file_content);
}
