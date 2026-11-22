#pragma once

#include <netinet/in.h>
#include <stdio.h>
#include <openssl/bio.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include "server.h"

typedef int (*response_func_ssl_t) (SSL* client_ssl, char* URI, char* request);

typedef struct {
    response_func_ssl_t response_function;
    char* URI;
} response_ssl_t;

typedef struct {
    struct sockaddr_in serv_addr;
    int sockfd;

    response_ssl_t* responses;
    int response_amnt;
    int allocated;

    SSL_CTX* ssl_context;
} server_ssl_t;

// server_ssl_t right now not thread safe. might cause data race depending on usage

server_ssl_t create_server_ssl(int port);

int start_server_ssl(server_ssl_t* server);
int start_server_ssl_multithread(server_ssl_t* server);

int standard_GET_response_ssl(SSL* client_ssl, char* URI, char* request);
int register_resource_ssl(server_ssl_t* server, char* URI, response_ssl_t resp);
void unregister_resource_ssl(server_ssl_t* server, char* URI);   // not implemented (just restart the server)