#define false 0
#define true 1

#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <errno.h>

#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

#include <sys/socket.h>
#include <sys/sendfile.h>
#include <netinet/in.h>
#include <arpa/inet.h>

int g_server = 0;
int g_client_connection = 0;

void sig_handler(int signo)
{
  if (signo == SIGINT)
  {
    printf("Stopping server ...\n");
    close(g_client_connection);
    close(g_server);
    printf("Server stopped.\n");
    exit(0);
  }
}

void hanlde_interrupt()
{
  if (signal(SIGINT, sig_handler) == SIG_ERR)
    printf("\ncan't catch SIGINT\n");
}

struct sockaddr_in get_server_config(int port)
{
  struct sockaddr_in addr = {
      AF_INET,
      htons(port),
      0};
  return addr;
}

int get_file_size(char *file_path)
{
  FILE *file = fopen(file_path, "r");
  fseek(file, 0, SEEK_END);
  int size = ftell(file);
  fseek(file, 0, SEEK_SET);
  fclose(file);
  return size;
}

void start_server()
{
  const int AUTO_CHOOSE_PROTOCOL = 0;
  const int MAX_CONNECTIONS = 10;
  const int optVal = 1;
  const socklen_t optLen = sizeof(optVal);

  printf("Starting server ...\n");

  // initialize server on port 8080
  if ((g_server = socket(AF_INET, SOCK_STREAM, AUTO_CHOOSE_PROTOCOL)) == -1)
  {
    printf("Error creating socket\n");
    exit(1);
  }

  // set socket options for unbinding the port by process termination
  if (setsockopt(g_server, SOL_SOCKET, SO_REUSEADDR, (void *)&optVal, optLen) == -1)
  {
    close(g_server);
    printf("Error setting socket options\n");
    exit(1);
  }

  // get configuration for server, e.g. port
  struct sockaddr_in addr = get_server_config(8080);

  // bind the server to the port
  if (bind(g_server, &addr, sizeof(addr)) == -1)
  {
    close(g_server);
    if (errno == EADDRINUSE)
      printf("Port is already in use\n");
    printf("Error binding to port\n");
    exit(1);
  }

  // listen to the port
  if (listen(g_server, MAX_CONNECTIONS) == -1)
  {
    close(g_server);
    printf("Error listening to port\n");
    exit(1);
  }

  printf("Server started.\n");
}

void handle_requests()
{
  const int NO_OPTIONS = 0;
  const char *STOP_COMMAND = "stop"; // stop server
  const char *STOP_MESSAGE = "Server stopped\n";
  char request_data[512] = {0};

  while (true)
  {
    // clear the buffer for requests
    memset(&request_data[0], 0, sizeof(request_data));

    // accept client connection
    if ((g_client_connection = accept(g_server, 0, 0)) == -1)
    {
      printf("Error accepting client\n");
      return;
    }

    // read the request from the client
    if (recv(g_client_connection, request_data, sizeof(request_data), NO_OPTIONS) == -1)
    {
      close(g_client_connection);
      printf("Error reading from client\n");
      return;
    }

    // GET /file.html .....
    // Remove "GET /" from the request_data and terminate string at first space
    // to get the file name
    char *file_path = request_data + 5;
    *strchr(file_path, ' ') = 0;

    // if the file name is "stop" then stop the server
    if (strcmp(file_path, STOP_COMMAND) == 0)
    {
      send(g_client_connection, STOP_MESSAGE, sizeof(STOP_MESSAGE), NO_OPTIONS);
      close(g_client_connection);
      printf("Stop receiving ...\n");
      break;
    }

    // open the file and send it to the client
    int requested_file_size = get_file_size(file_path);
    int requested_file_fd = open(file_path, O_RDONLY);

    sendfile(g_client_connection, requested_file_fd, 0, requested_file_size);

    // close the file client connection
    close(requested_file_fd);
    close(g_client_connection);
  }
}

void main()
{
  hanlde_interrupt();

  // start the server
  start_server();
  handle_requests();

  // close the server
  printf("Stopping server ...\n");
  close(g_server);
  printf("Server stopped.\n");
  exit(0);
}