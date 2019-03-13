#include <arpa/inet.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>


#include "libhttp.h"
#include "wq.h"

/*
 * Global configuration variables.
 * You need to use these in your implementation of handle_files_request and
 * handle_proxy_request. Their values are set up in main() using the
 * command line arguments (already implemented for you).
 */
wq_t work_queue;
int num_threads;
int server_port;
char *server_files_directory;
char *server_proxy_hostname;
int server_proxy_port;
#define MAX_SIZE 1024

void http_reponse_not_found(int fd){
    http_start_response(fd, 404);
    http_send_header(fd, "Content-Type", "text/html");
    http_end_headers(fd);
    http_send_string(fd,
                     "<center>"
                     "<h1>File or Directory Don\'t exist</h1>"
                     "</center>");
}


void http_reponse_ok(int fd, char *path){
    http_start_response(fd, 200);
    http_send_header(fd, "Content-Type", http_get_mime_type(path));

    FILE* fp = fopen(path, "r");

    if (!fp) {
        return http_reponse_not_found(fd);
    }

    fseek(fp, 0, SEEK_END);
    int content_length = ftell(fp);
    char content_length_str[MAX_SIZE];
    sprintf(content_length_str, "%d", content_length);
    rewind(fp);
    http_send_header(fd, "Content-Length", content_length_str);
    http_end_headers(fd);

    char *body = (char *)malloc(sizeof(char) * content_length);
    fread(body, 1, content_length, fp);
    http_send_string(fd, body);
    fclose(fp);
    free(body);
    close(fd);
}

void http_response_list(int fd, char* absolute_path, char* relative_path) {
      DIR *d;
      struct dirent *dir;
      d = opendir(absolute_path);
      char result_buffer[MAX_SIZE];
      memset(result_buffer, '\0', MAX_SIZE);
      if (d) {
          strcpy(result_buffer, "<html><body><ul>");
          while ((dir = readdir(d)) != NULL) {
              char link_buffer[MAX_SIZE];
              char item_buffer[MAX_SIZE];
              memset(link_buffer, '\0', MAX_SIZE);
              memset(item_buffer, '\0', MAX_SIZE);
              strcpy(link_buffer, relative_path);
              strcat(link_buffer, "//");
              strcat(link_buffer, dir->d_name);
              sprintf(item_buffer, "<li><a href=\"%s\">%s</a></li>",link_buffer, dir->d_name);
              strcat(result_buffer, item_buffer); 
          }
          strcat(result_buffer, "</ul></body></html>");
          closedir(d);
          http_start_response(fd, 200);
          http_send_header(fd, "Content-Type", "text/html");
          http_end_headers(fd);
          http_send_string(fd, result_buffer);
          close(fd);
      }else{
          http_reponse_not_found(fd);
      }
}



/*
 * Reads an HTTP request from stream (fd), and writes an HTTP response
 * containing:
 *
 *   1) If user requested an existing file, respond with the file
 *   2) If user requested a directory and index.html exists in the directory,
 *      send the index.html file.
 *   3) If user requested a directory and index.html doesn't exist, send a list
 *      of files in the directory with links to each.
 *   4) Send a 404 Not Found response.
 */
void handle_files_request(int fd) {

  /*
   * TODO: Your solution for Task 1 goes here! Feel free to delete/modify *
   * any existing code.
   */
  printf("the thread number is %ld\n", pthread_self());
  struct http_request *request = http_request_parse(fd);
  if(!request){
      printf("%ld gets a wrong request\n", pthread_self());
      return http_reponse_not_found(fd);
  }
  if(strcmp(request->method, "GET") != 0){
      printf("%ld don\'t know the request method\n", pthread_self());
      return http_reponse_not_found(fd); 
  }
  
  printf("%ld gets a request and prepares to response\n", pthread_self());

  struct stat path_stat;
  char file_path[MAX_SIZE];
  file_path[0] = '\0';
  strcat(file_path, server_files_directory);
  strcat(file_path, request->path);
  
  if(stat(file_path, &path_stat) == -1){
      return http_reponse_not_found(fd);
  }
   
  if(S_ISREG(path_stat.st_mode)){
      http_reponse_ok(fd, file_path);
  }else if(S_ISDIR(path_stat.st_mode)){
      char html_path[MAX_SIZE];
      memset(html_path, '\0', MAX_SIZE);
      strcpy(html_path, file_path);
      strcat(html_path, "//index.html");
      FILE* fp = fopen(html_path, "r");
      
     if(fp){
          http_reponse_ok(fd, html_path);
      }else{
          http_response_list(fd, file_path, request->path);
      }
  }else{
      http_reponse_not_found(fd);
  }
}


void *thread_communication(void *args){
    int *fd_list= args;
    int fd_from = fd_list[0];
    int fd_to = fd_list[1];
    char message[MAX_SIZE];
    memset(message, '\0', MAX_SIZE);
    int size;
    while((size = read(fd_from, message, MAX_SIZE)) > 0){
        write(fd_to, message, size);
    }
    return NULL;
}

/*
 * Opens a connection to the proxy target (hostname=server_proxy_hostname and
 * port=server_proxy_port) and relays traffic to/from the stream fd and the
 * proxy target. HTTP requests from the client (fd) should be sent to the
 * proxy target, and HTTP responses from the proxy target should be sent to
 * the client (fd).
 *
 *   +--------+     +------------+     +--------------+
 *   | client | <-> | httpserver | <-> | proxy target |
 *   +--------+     +------------+     +--------------+
 */
void handle_proxy_request(int fd) {

  /*
  * The code below does a DNS lookup of server_proxy_hostname and 
  * opens a connection to it. Please do not modify.
  */

  struct sockaddr_in target_address;
  memset(&target_address, 0, sizeof(target_address));
  target_address.sin_family = AF_INET;
  target_address.sin_port = htons(server_proxy_port);

  struct hostent *target_dns_entry = gethostbyname2(server_proxy_hostname, AF_INET);

  int client_socket_fd = socket(PF_INET, SOCK_STREAM, 0);
  if (client_socket_fd == -1) {
    fprintf(stderr, "Failed to create a new socket: error %d: %s\n", errno, strerror(errno));
    exit(errno);
  }

  if (target_dns_entry == NULL) {
    fprintf(stderr, "Cannot find host: %s\n", server_proxy_hostname);
    exit(ENXIO);
  }

  char *dns_address = target_dns_entry->h_addr_list[0];

  memcpy(&target_address.sin_addr, dns_address, sizeof(target_address.sin_addr));
  int connection_status = connect(client_socket_fd, (struct sockaddr*) &target_address,
      sizeof(target_address));

  if (connection_status < 0) {
    /* Dummy request parsing, just to be compliant. */
    http_request_parse(fd);

    http_start_response(fd, 502);
    http_send_header(fd, "Content-Type", "text/html");
    http_end_headers(fd);
    http_send_string(fd, "<center><h1>502 Bad Gateway</h1><hr></center>");
    return;

  }

  /* 
  * TODO: Your solution for task 3 belongs here! 
  */
  pthread_t proxy_server, proxy_client;
  int proxy_server_args[] = {client_socket_fd, fd};
  int proxy_client_args[] = {fd, client_socket_fd};
  pthread_create(&proxy_server, NULL, thread_communication, proxy_server_args);
  pthread_create(&proxy_client, NULL, thread_communication, proxy_client_args);
  pthread_join(proxy_client, NULL);
  pthread_join(proxy_server, NULL);
  printf("Finish for one connection \n");
  close(fd);
  close(client_socket_fd);
}

void *thread_wait(void *args){
    void (*request_handler)(int) = args;
    int client_socket_number;
    while(1){
        client_socket_number = wq_pop(&work_queue);
        request_handler(client_socket_number);
        close(client_socket_number);
    }
    return NULL;
}

void init_thread_pool(int num_threads, void (*request_handler)(int)) {
  /*
   * TODO: Part of your solution for Task 2 goes here!
   */
    wq_init(&work_queue);
    pthread_t p[num_threads];
    for(int i = 0; i < num_threads; i++){
        pthread_create(&(p[i]), NULL, thread_wait, request_handler);
    }
}

/*
 * Opens a TCP stream socket on all interfaces with port number PORTNO. Saves
 * the fd number of the server socket in *socket_number. For each accepted
 * connection, calls request_handler with the accepted fd number.
 */
void serve_forever(int *socket_number, void (*request_handler)(int)) {

  struct sockaddr_in server_address, client_address;
  size_t client_address_length = sizeof(client_address);
  int client_socket_number;

  *socket_number = socket(PF_INET, SOCK_STREAM, 0);
  if (*socket_number == -1) {
    perror("Failed to create a new socket");
    exit(errno);
  }

  int socket_option = 1;
  if (setsockopt(*socket_number, SOL_SOCKET, SO_REUSEADDR, &socket_option,
        sizeof(socket_option)) == -1) {
    perror("Failed to set socket options");
    exit(errno);
  }

  memset(&server_address, 0, sizeof(server_address));
  server_address.sin_family = AF_INET;
  server_address.sin_addr.s_addr = INADDR_ANY;
  server_address.sin_port = htons(server_port);

  if (bind(*socket_number, (struct sockaddr *) &server_address,
        sizeof(server_address)) == -1) {
    perror("Failed to bind on socket");
    exit(errno);
  }

  if (listen(*socket_number, 1024) == -1) {
    perror("Failed to listen on socket");
    exit(errno);
  }

  printf("Listening on port %d...\n", server_port);

  init_thread_pool(num_threads, request_handler);

  while (1) {
    client_socket_number = accept(*socket_number,
        (struct sockaddr *) &client_address,
        (socklen_t *) &client_address_length);
    if (client_socket_number < 0) {
      perror("Error accepting socket");
      continue;
    }

    printf("Accepted connection from %s on port %d\n",
        inet_ntoa(client_address.sin_addr),
        client_address.sin_port);

    // TODO: Change me?
    wq_push(&work_queue, client_socket_number);
  }

  shutdown(*socket_number, SHUT_RDWR);
  close(*socket_number);
}

int server_fd;
void signal_callback_handler(int signum) {
  printf("Caught signal %d: %s\n", signum, strsignal(signum));
  printf("Closing socket %d\n", server_fd);
  if (close(server_fd) < 0) perror("Failed to close server_fd (ignoring)\n");
  exit(0);
}

char *USAGE =
  "Usage: ./httpserver --files www_directory/ --port 8000 [--num-threads 5]\n"
  "       ./httpserver --proxy inst.eecs.berkeley.edu:80 --port 8000 [--num-threads 5]\n";

void exit_with_usage() {
  fprintf(stderr, "%s", USAGE);
  exit(EXIT_SUCCESS);
}

int main(int argc, char **argv) {
  signal(SIGINT, signal_callback_handler);

  /* Default settings */
  server_port = 8000;
  void (*request_handler)(int) = NULL;

  int i;
  for (i = 1; i < argc; i++) {
    if (strcmp("--files", argv[i]) == 0) {
      request_handler = handle_files_request;
      free(server_files_directory);
      server_files_directory = argv[++i];
      if (!server_files_directory) {
        fprintf(stderr, "Expected argument after --files\n");
        exit_with_usage();
      }
    } else if (strcmp("--proxy", argv[i]) == 0) {
      request_handler = handle_proxy_request;

      char *proxy_target = argv[++i];
      if (!proxy_target) {
        fprintf(stderr, "Expected argument after --proxy\n");
        exit_with_usage();
      }

      char *colon_pointer = strchr(proxy_target, ':');
      if (colon_pointer != NULL) {
        *colon_pointer = '\0';
        server_proxy_hostname = proxy_target;
        server_proxy_port = atoi(colon_pointer + 1);
      } else {
        server_proxy_hostname = proxy_target;
        server_proxy_port = 80;
      }
    } else if (strcmp("--port", argv[i]) == 0) {
      char *server_port_string = argv[++i];
      if (!server_port_string) {
        fprintf(stderr, "Expected argument after --port\n");
        exit_with_usage();
      }
      server_port = atoi(server_port_string);
    } else if (strcmp("--num-threads", argv[i]) == 0) {
      char *num_threads_str = argv[++i];
      if (!num_threads_str || (num_threads = atoi(num_threads_str)) < 1) {
        fprintf(stderr, "Expected positive integer after --num-threads\n");
        exit_with_usage();
      }
    } else if (strcmp("--help", argv[i]) == 0) {
      exit_with_usage();
    } else {
      fprintf(stderr, "Unrecognized option: %s\n", argv[i]);
      exit_with_usage();
    }
  }

  if (server_files_directory == NULL && server_proxy_hostname == NULL) {
    fprintf(stderr, "Please specify either \"--files [DIRECTORY]\" or \n"
                    "                      \"--proxy [HOSTNAME:PORT]\"\n");
    exit_with_usage();
  }

  serve_forever(&server_fd, request_handler);

  return EXIT_SUCCESS;
}
