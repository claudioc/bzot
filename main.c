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

#define MY_PORT 8080
#define MY_MAX_CONNECTIONS 3

#define USE_NON_BLOCKING_SOCKETS false

static bool set_sock_opt(int socket, int option, bool enable) {
  int yes = (enable ? 1 : 0);
  int err = setsockopt(socket, SOL_SOCKET, option,  &yes, sizeof(yes));
  return !err;
}

void handle_connection(int);
void cleanup_worker(int);

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

// char *read_first_line(int, char *, size_t);
// char **read_headers(int, char *);
// char **read_body(int, char *);

void handle_connection(int sockfd) {
  size_t BUFSIZE = 20;
  ssize_t received;
  char buffer[BUFSIZE + 1];

  // TODO skip the first empty lines, if any
  // char *first_line = read_first_line(sockfd, buffer);
  // read_headers(sockfd, buffer);
  // read_body(sockfd, buffer);
  received = read(sockfd, buffer, BUFSIZE);
  if ('\r' == buffer[received - 1]) {
    read(sockfd, buffer + BUFSIZE, 1);
  }

  char *line_start = buffer;
  char *line_end;
  while ((line_end = (char *)memchr((void *)line_start, '\n', BUFSIZE - (line_start - buffer)))) {
    *line_end = 0;
    puts("[");
    puts(line_start);
    puts("]");
    line_start = line_end + 1;
  }

  memset(buffer, 0x00, BUFSIZE);
  while ((received = read(sockfd, buffer, BUFSIZE))) {
    printf("Read %d chars", (unsigned int)received);
    buffer[received] = 0x00;
    puts(buffer);
    memset(buffer, 0x00, sizeof(buffer));
    // TODO handle the case where telnet must send \n\n
  }

  close(sockfd);
  puts("Handler is done");
  exit(0);
}

// char *read_first_line(int sockfd, char *buffer, size_t max) {
//   return NULL;
// }

// char **read_headers(int sockfd, char *buffer) {
//   return NULL;
// }

// char **read_body(int sockfd, char *buffer) {
//   return NULL;
// }
