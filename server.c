#include "server.h"
#include <pthread.h>
#include <stdlib.h>

static char HTTP_HTML_RESP[] = "HTTP/1.1 200 OK\r\n\
Content-Type: text/html; charset=UTF-8\r\n\r\n";

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

int read_file(char* filename, char* buffer, size_t buffersz)
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

void* handle_client(void* vptr_client_sockfd)
{
    int client_sockfd = *((int*) vptr_client_sockfd);
    printf("Filedescriptor: %i", client_sockfd);
    char buffer[2048];
    bzero(buffer, sizeof(buffer));

    while(1){
        //  implement this correctly
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

        // choose correct website here
        memcpy(buffer, HTTP_HTML_RESP, sizeof(HTTP_HTML_RESP));
        int sz = read_file("../resources/index.html", buffer+sizeof(HTTP_HTML_RESP), sizeof(buffer)-sizeof(HTTP_HTML_RESP));
        amnt = send(client_sockfd, buffer, sizeof(HTTP_HTML_RESP) + sz, MSG_NOSIGNAL);
        if(amnt < 0){
            perror("Could not write message to client!");
            break;
        }
    }

    close(client_sockfd);
    free(vptr_client_sockfd);
    printf("Finished client handling.\n\n");
    return 0;
}

int start_server(server_t server)
{
    if(server.sockfd < 0) return -1;

    while(1){
        struct sockaddr_in client_addr;
        int clilen = sizeof(client_addr);

        int* client_sockfd = malloc(sizeof(int));
        *client_sockfd = accept(server.sockfd, (struct sockaddr*) &client_addr, &clilen);
        if(client_sockfd < 0){
            perror("Could not accept client!");
        }

        handle_client(client_sockfd);
    }

    return 0;
}

int start_server_multithread(server_t server)
{
    if(server.sockfd < 0) return -1;

    while(1){
        struct sockaddr_in client_addr;
        int clilen = sizeof(client_addr);

        int* client_sockfd = malloc(sizeof(int));
        *client_sockfd = accept(server.sockfd, (struct sockaddr*) &client_addr, &clilen);
        if(client_sockfd < 0){
            perror("Could not accept client!");
        }

        pthread_t thread;
        pthread_create(&thread, 0, handle_client, client_sockfd);
        pthread_detach(thread);
    }

    return 0;
}
