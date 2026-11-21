#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <stdio.h>

typedef enum{
    TCP,
    UDP
} SOCKET_TYPE;

typedef int (*response_func_t) (int clientsockfd, char* URI, char* request);

typedef struct {
    response_func_t response_function;
    char* URI;
} response_t;

typedef struct {
    struct sockaddr_in serv_addr;
    int sockfd;

    response_t* responses;
    int response_amnt;
    int allocated;
} server_t;

server_t create_server(int port);

int start_server(server_t* server);
int start_server_multithread(server_t* server);

int standard_GET_response(int clientsockfd, char* URI, char* request);
int register_resource(server_t* server, char* URI, response_t resp);
void unregister_resource(server_t* server, char* URI);   // not implemented (just restart the server)