#include "server.h"
#include "http.h"
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>

static char resource_folder[] = "../resources/";

static char HTTP_HTML_RESP[] = "HTTP/1.1 200 OK\r\n\
Content-Type: text/html; charset=UTF-8\r\n\r\n";

int standard_GET_response(int clientsockfd, char* URI, char* request)
{
    char* filename = 0;
    char* fileURI = 0;
    char* filecont = 0;
    char* response_header_buffer = 0;
    char* buffer = 0;

    if(!strcmp(URI, "/")){
        filename = "index.html";
    } else { //  better not keep secret stuff in the resource folder 
        if(strstr(URI, "..")) return 1; // make sure we dont escape resource folder (is this enough idk?)

        filename = malloc(strlen(URI) + strlen(".html") + 1);
        memcpy(filename, URI, strlen(URI));
        memcpy(filename + strlen(URI), ".html", strlen(".html"));
        filename[strlen(URI) + strlen(".html")] = '\0';
    }

    fileURI = malloc(strlen(resource_folder) + strlen(filename) + 1);
    if(!fileURI) return 1;

    memcpy(fileURI, resource_folder, strlen(resource_folder));
    memcpy(fileURI+strlen(resource_folder), filename, strlen(filename));
    fileURI[strlen(resource_folder) + strlen(filename)] = '\0';

    FILE* fptr = fopen(fileURI, "r");
    if(!fptr) {
        perror("Could not open file");
        goto fail;
    }

    fseek(fptr, 0, SEEK_END);
    long fsize = ftell(fptr);
    fseek(fptr, 0, SEEK_SET);  

    filecont = malloc(fsize + 1);
    if(!filecont) {
        fclose(fptr);
        perror("Could not allocate memory for file content");
        goto fail;
    }

    fread(filecont, fsize, 1, fptr);
    filecont[fsize] = 0;

    fclose(fptr);

    HTTP_response_header_t response_header = {.status = OK_200};
    char num_buffer[50];
    sprintf(num_buffer, "%li", fsize);
    HTTP_pair_t content_length = {"Content-Length", num_buffer};
    response_header.fields = &content_length;
    response_header.fields_len = 1;
    response_header_buffer = HTTP_create_response(response_header);

    int buffer_sz = strlen(response_header_buffer) + strlen(filecont) + 1;
    buffer = malloc(buffer_sz);
    if(!buffer) {
        perror("Could not allocate memory for response buffer");
        goto fail;
    }
    memcpy(buffer, response_header_buffer, strlen(response_header_buffer));
    memcpy(buffer+strlen(response_header_buffer), filecont, strlen(filecont));
    buffer[strlen(response_header_buffer) + strlen(filecont)] = '\0'; // unnecessary use content-length

    int amnt = 0;
    while(amnt < buffer_sz){
        amnt += send(clientsockfd, buffer + amnt, buffer_sz - amnt, MSG_NOSIGNAL);
        if(amnt <= 0){
            perror("Error when sending message to client");
            goto fail;
        } 
    }

    free(fileURI);
    free(filecont);
    free(response_header_buffer);
    free(buffer);
    return 0;

fail:
    free(fileURI);
    free(filecont);
    free(response_header_buffer);
    free(buffer);
    return 1;
}

int register_resource(server_t* server, char* URI, response_t resp)
{
    if(server->response_amnt == server->allocated){
        response_t* temp = realloc(server->responses, 2 *(server->allocated+1) * sizeof(response_t));
        if(!temp) return 1;

        server->allocated = 2*(server->allocated+1);
        server->responses = temp;
    }

    server->responses[server->response_amnt] = resp;
    server->response_amnt++;
    return 0;
}


server_t create_server(int port)
{
    server_t server = {0};

    struct sockaddr_in serv_addr = {
        .sin_family = AF_INET, 
        .sin_port = htons(port), 
        .sin_addr = 0
    };
    server.serv_addr = serv_addr;

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    server.sockfd = sockfd;
    if(sockfd < 0){
        perror("Could not create server socket!");
        return server;
    }

    if(bind(sockfd, (struct sockaddr*) &server.serv_addr, sizeof(server.serv_addr)) < 0){
        perror("Could not bind server socket!");
        close(server.sockfd);
        server.sockfd = -1;
        return server;
    }

    if(listen(server.sockfd, 5) < 0) {
        perror("Could not start listening on server socket!");
        close(server.sockfd);
        server.sockfd = -1;
        return server;
    }

    printf("Successfully created server on socket %d\n", server.sockfd);

    return server;
}

static int read_file(char* filename, char* buffer, size_t buffersz)
{
    FILE* fptr = fopen(filename, "r");
    if(fptr == 0){
        perror("Could not open file!");
        printf("File path: %s\n", filename);
        return -1;
    }

    int amnt = 0;
    int offset = 0;

    amnt = fread(buffer, 1, buffersz, fptr);
    if(amnt < 0) perror("Could not read file!");

    fclose(fptr);
    return amnt;
}

static response_t* find_response(server_t* server, char* URI)
{
    for(int i = 0; i < server->response_amnt; i++){
        if(!strcmp(URI, server->responses[i].URI)) return &server->responses[i];
    }
    return 0;
}

static int respond_client(server_t* server, int clientsockfd, char* request)
{
    HTTP_request_header_t request_header = HTTP_parse_request(request);
    if(request_header.request_type == INVALID) return 1;

    response_t* response = find_response(server, request_header.URI);
    if(!response) return 1;

    return response->response_function(clientsockfd, response->URI, request);
}

typedef struct {
    int clientfd;
    server_t* server;
} clientfd_server_pair_t;

static void* handle_client(void* vptr_clientfd_server_pair)
{
    clientfd_server_pair_t temp = *((clientfd_server_pair_t*) vptr_clientfd_server_pair);
    int client_sockfd = temp.clientfd;
    server_t* ptr_server = temp.server;
    free(vptr_clientfd_server_pair);

    printf("Filedescriptor: %i", client_sockfd);
    char buffer[2048];
    bzero(buffer, sizeof(buffer));

    while(1){
        int amnt = recv(client_sockfd, buffer, sizeof(buffer), 0); 

        if(amnt == 0){
            printf("Client disconnected");
            break;
        }

        if(amnt < 0){
            perror("Could not read client message!");
            break;
        }

        printf("Received the following message:\n");
        printf("%s\n", buffer);

        if(respond_client(ptr_server, client_sockfd, buffer)){
            printf("Error when responding to client");
        }
    }

    close(client_sockfd);
    printf("Finished client handling.\n\n");
    return 0;
}

int start_server(server_t* server)
{
    if(server->sockfd < 0) return -1;

    while(1){
        struct sockaddr_in client_addr;
        int clilen = sizeof(client_addr);

        int* client_sockfd = malloc(sizeof(int));
        if(!client_sockfd) continue;
        *client_sockfd = accept(server->sockfd, (struct sockaddr*) &client_addr, &clilen);
        if(client_sockfd < 0){
            perror("Could not accept client!");
        }

        handle_client(client_sockfd);
    }

    return 0;
}

int start_server_multithread(server_t* server)
{
    if(server->sockfd < 0) return -1;

    while(1){
        struct sockaddr_in client_addr;
        int clilen = sizeof(client_addr);

        int* client_sockfd = malloc(sizeof(int));
        if(!client_sockfd) continue;
        *client_sockfd = accept(server->sockfd, (struct sockaddr*) &client_addr, &clilen);
        if(client_sockfd < 0){
            perror("Could not accept client!");
        }

        pthread_t thread;
        clientfd_server_pair_t* data = malloc(sizeof(clientfd_server_pair_t));    // free called in new thread
        if(!data) continue;
        *data = (clientfd_server_pair_t){.clientfd = *client_sockfd, .server = server};

        pthread_create(&thread, 0, handle_client, data);
        pthread_detach(thread);
    }

    return 0;
}
