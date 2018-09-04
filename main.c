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
  const int sockaddr_in_len = sizeof(struct sockaddr_in);

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

  int bound = bind(server_sockfd, (struct sockaddr *)&server, sizeof(server));
  if (bound < 0) {
    fprintf(stderr, "Bind failed on endpoint socket: %s\n", strerror(errno));
    return EXIT_FAILURE;
  }

  int listened = listen(server_sockfd, MY_MAX_CONNECTIONS);
  if (listened == -1) {
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
    if (child_pid == -1) {
      fprintf(stderr, "Fork failed on accept: %s\n", strerror(errno));
      return EXIT_FAILURE;
    }

    if (child_pid == 0) {
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

void handle_connection(int sockfd) {
  const unsigned int BUFSIZE = 20;
  char buffer[BUFSIZE];
  int received;

  memset(buffer, 0x00, BUFSIZE);
  while ((received = read(sockfd, buffer, BUFSIZE))) {
    printf("Read %d chars", received);
    buffer[received] = 0x00;
    puts(buffer);
    memset(buffer, 0x00, sizeof(buffer));
    // TODO handle the case where telnet must send \n\n
  }

  close(sockfd);
  puts("Handler is done");
  exit(0);
}
