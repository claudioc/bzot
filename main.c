/**
 * Bzot – The useless http server
 * Copyright (C) 2018, Claudio Cicali <claudio.cicali@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdbool.h>
#include <fcntl.h>

#define MY_PORT 8081
#define MY_MAX_CONNECTIONS 3

#define USE_NON_BLOCKING_SOCKETS false

// Using lldb on OSX, we can't debug child processes with VSC
#define USE_MULTIPROCESS true

static bool set_sock_opt(int socket, int option, bool enable) {
  int yes = (enable ? 1 : 0);
  int err = setsockopt(socket, SOL_SOCKET, option,  &yes, sizeof(yes));
  return !err;
}

void handle_connection(int);
void cleanup_worker(int);
void dump_buffer(char *, int);
bool binstr(const void *, size_t, const char *);

int main(int argc, char **argv) {
  int server_sockfd;
  int client_sockfd;
  struct sockaddr_in server;
  struct sockaddr_in client;
  size_t sockaddr_in_len = sizeof(struct sockaddr_in);

  (void)argc;
  (void)argv;

  server_sockfd = socket(AF_INET, SOCK_STREAM, 0);

  if (server_sockfd == -1) {
    fprintf(stderr, "Can't open server socket: %s\n", strerror(errno));
    return EXIT_FAILURE;
  }

  set_sock_opt(server_sockfd, SO_REUSEADDR, true);
  #ifdef SO_REUSEPORT
  set_sock_opt(server_sockfd, SO_REUSEPORT, true);
  #endif

  // Set the socket to be non-blocking.
  // If this option is activated, remember to also set the client
  // socket to be non-blocking upon connection.
  #if USE_NON_BLOCKING_SOCKETS
  fcntl(server_sockfd, F_SETFL, fcntl(server_sockfd, F_GETFL, 0) | O_NONBLOCK);
  #endif

  memset(&server, 0, sizeof(server));
  server.sin_family = AF_INET;
  server.sin_addr.s_addr = INADDR_ANY;
  server.sin_port = htons(MY_PORT);

  if (-1 == bind(server_sockfd, (struct sockaddr *)&server, sizeof(server))) {
    fprintf(stderr, "Bind failed on endpoint socket: %s\n", strerror(errno));
    return EXIT_FAILURE;
  }

  if (-1 == listen(server_sockfd, MY_MAX_CONNECTIONS)) {
    fprintf(stderr, "Listen failed on endpoint socket: %s\n", strerror(errno));
    return EXIT_FAILURE;
  }

  puts("Waiting for incoming connections…");

  while (true) {
    memset(&client, 0x00, sizeof(client));
    client_sockfd = accept(server_sockfd, (struct sockaddr *)&client, (socklen_t*)&sockaddr_in_len);
    if (client_sockfd == -1) {
      // It is advisable to handle return values < 0 for accept()/read()/write() properly
      // by checking errno. You might just be getting EAGAIN or EINTR which basically means
      // "retry the same call"
      fprintf(stderr, "Accept failed on server socket: %s\n", strerror(errno));
      return EXIT_FAILURE;
    }
    puts("Connection accepted");

    #if USE_MULTIPROCESS
    signal(SIGCHLD, cleanup_worker);
    int child_pid = fork();
    if (-1 == child_pid) {
      fprintf(stderr, "Fork failed on accept: %s\n", strerror(errno));
      return EXIT_FAILURE;
    }

    if (0 == child_pid) {
      // As a child, we don't need the server socket anymore
      close(server_sockfd);
      handle_connection(client_sockfd);
    } else {
      // As a server, we don't need the client socket anymore
      close(client_sockfd);
    }
    #else
      handle_connection(client_sockfd);
    #endif

    puts("Ready for more");
  }

  return EXIT_SUCCESS;
}

void cleanup_worker(int signal) {
  (void)signal;
  // waitpid() might overwrite errno, so we save and restore it:
  int saved_errno = errno;
  while (waitpid((pid_t) (-1), 0, WNOHANG) > 0);
  errno = saved_errno;
}

void handle_connection(int sockfd) {
  size_t CHUNKSIZE = 20;
  size_t read_size = 0;
  size_t total_read = 0;
  char *buffer = (char *)malloc(CHUNKSIZE);

  while ((read_size = read(sockfd, buffer + total_read, CHUNKSIZE)) > 0) {
    total_read += read_size;
    // A '0d0a0d0a' in the buffer signals the end of the request
    buffer[total_read] = 0x00;
    if (strstr(buffer, "\r\n\r\n") != NULL) {
      break;
    }

    if (total_read > CHUNKSIZE) {
      buffer = realloc((void *)buffer, CHUNKSIZE + 1);
    }
  }

  unsigned char ch;
  for (unsigned int i = 0; i < total_read; i++) {
    ch = buffer[i];
    putchar(ch);
  }

  const char *reply = "HTTP/1.1 200 OK\r\n\r\nHello from bzot!\r\n";
  write(sockfd, reply, strlen(reply));

  free(buffer);
  close(sockfd);

  puts("Handler is done");
  #if USE_MULTIPROCESS
  exit(0);
  #endif
}

void dump_buffer(char *buffer, int max_length) {
  for (int i = 0; i < max_length; i++) {
    printf("[%02x]", buffer[i]);
  }
  printf("\n");
}
