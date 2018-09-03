#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdbool.h>

#define MY_PORT 8080
#define MY_MAX_CONNECTIONS 3

static bool set_sock_opt(int socket, int option, bool enable) {
	int yes = (enable ? 1 : 0);
	int err = setsockopt(socket, SOL_SOCKET, option,  &yes, sizeof(yes));
	return !err;
}

void handle_connection(int);

int main(int argc, char **argv) {
  int server_sockfd;
  int client_sockfd;
  struct sockaddr_in server;
  struct sockaddr_in client;
  const int sockaddr_in_len = sizeof(struct sockaddr_in);
  
  // SO_REUSEADDRESS
  // SOCK_NONBLOCK
  server_sockfd = socket(AF_INET, SOCK_STREAM, 0);

  if (server_sockfd == -1) {
    fputs("Can't open server socket", stderr);
    return EXIT_FAILURE;
  }

  set_sock_opt(server_sockfd, SO_REUSEADDR, true);
  #ifdef SO_REUSEPORT
  set_sock_opt(server_sockfd, SO_REUSEPORT, true);
  #endif

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
    char *client_ip = inet_ntoa(client.sin_addr);
    int client_port = ntohs(client.sin_port);

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
      waitpid(child_pid, NULL, 0);
    }
  }

  return EXIT_SUCCESS;
}

void handle_connection(int sockfd) {
  char *message = "Greetings! I am your connection handler\n";
  write(sockfd, message, strlen(message));
  close(sockfd);
  puts("Handler is done");
  exit(0);
}
