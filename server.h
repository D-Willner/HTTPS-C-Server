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

typedef void* (*response_t) (int, char*);

typedef struct {
    struct sockaddr_in serv_addr;
    int sockfd;
} server_t;

server_t create_server(int port);

int start_server(server_t server);

int start_server_multithread(server_t server);

void register_resource(server_t server, char* URI, response_t resp);

void unregister_resource(server_t server, char* URI);