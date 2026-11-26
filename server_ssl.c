#include "server_ssl.h"
#include "http.h"
#include <pthread.h>
#include <stdlib.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <stdio.h>

static char resource_folder[] = "../resources/";

int standard_GET_response_ssl(SSL* client_ssl, char* URI, char* request)
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

    if(fseek(fptr, 0, SEEK_END) != 0){
        fclose(fptr);
        goto fail;
    }
    long fsize = ftell(fptr);
    if(fsize < 0 || fseek(fptr, 0, SEEK_SET) != 0){
        fclose(fptr);
        goto fail;
    }

    filecont = malloc(fsize + 1);
    if(!filecont) {
        fclose(fptr);
        perror("Could not allocate memory for file content");
        goto fail;
    }

    size_t body_size = fread(filecont, 1, fsize, fptr);
    fclose(fptr);
    if(body_size != (size_t)fsize) goto fail;

    HTTP_response_header_t response_header = {.status = OK_200};
    char num_buffer[50];
    sprintf(num_buffer, "%li", fsize);
    HTTP_pair_t content_length = {"Content-Length", num_buffer};
    response_header.fields = &content_length;
    response_header.fields_len = 1;
    response_header_buffer = HTTP_create_response(response_header);

    if(!response_header_buffer) goto fail;

    size_t header_size = strlen(response_header_buffer);
    size_t buffer_sz = header_size + body_size;
    buffer = malloc(buffer_sz);
    if(!buffer) {
        perror("Could not allocate memory for response buffer");
        goto fail;
    }
    memcpy(buffer, response_header_buffer, header_size);
    memcpy(buffer + header_size, filecont, body_size);

    size_t written;
    if(SSL_write_ex(client_ssl, buffer, buffer_sz, &written) != 1){
        fprintf(stderr, "Could not send TLS response\n");
        ERR_print_errors_fp(stderr);
        goto fail;
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

int register_resource_ssl(server_ssl_t* server, char* URI, response_ssl_t resp)
{
    if(server->response_amnt == server->allocated){
        response_ssl_t* temp = realloc(server->responses, 2 *(server->allocated+1) * sizeof(response_ssl_t));
        if(!temp) return 1;

        server->allocated = 2*(server->allocated+1);
        server->responses = temp;
    }

    server->responses[server->response_amnt] = resp;
    server->response_amnt++;
    return 0;
}


server_ssl_t create_server_ssl(int port)
{
    server_ssl_t server = {.sockfd = -1};
    long opts = 0;

    server.ssl_context = SSL_CTX_new(TLS_server_method());
    if(server.ssl_context == 0) {
        fprintf(stderr, "Failed to create the TLS context\n");
        goto tls_fail;
    }

    if(!SSL_CTX_set_min_proto_version(server.ssl_context, TLS1_2_VERSION)) {
        fprintf(stderr, "Failed to set the minimum TLS version\n");
        goto tls_fail;
    }

    //opts = SSL_OP_IGNORE_UNEXPECTED_EOF;
    opts |= SSL_OP_NO_RENEGOTIATION;
    opts |= SSL_OP_CIPHER_SERVER_PREFERENCE;
    SSL_CTX_set_options(server.ssl_context, opts);

    if(SSL_CTX_use_certificate_chain_file(server.ssl_context, "chain.pem") != 1){
        fprintf(stderr, "Failed to load the server certificate from chain.pem\n");
        goto tls_fail;
    }

    if(SSL_CTX_use_PrivateKey_file(server.ssl_context, "pkey.pem", SSL_FILETYPE_PEM) != 1){
        fprintf(stderr, "Failed to load the server private key from pkey.pem\n");
        goto tls_fail;
    }

    if(SSL_CTX_check_private_key(server.ssl_context) != 1){
        fprintf(stderr, "The server certificate and private key do not match\n");
        goto tls_fail;
    }

    SSL_CTX_set_verify(server.ssl_context, SSL_VERIFY_NONE, NULL);

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
        goto fail;
    }

    if(bind(sockfd, (struct sockaddr*) &server.serv_addr, sizeof(server.serv_addr)) < 0){
        perror("Could not bind server socket!");
        goto fail;
    }

    if(listen(server.sockfd, 5) < 0) {
        perror("Could not start listening on server socket!");
        goto fail;
    }

    printf("Successfully created server on socket %d\n", server.sockfd);

    return server;

tls_fail:
    ERR_print_errors_fp(stderr);
fail:
    if(server.sockfd >= 0) close(server.sockfd);
    SSL_CTX_free(server.ssl_context);
    return (server_ssl_t){.sockfd = -1};
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

static response_ssl_t* find_response(server_ssl_t* server, char* URI)
{
    for(int i = 0; i < server->response_amnt; i++){
        if(!strcmp(URI, server->responses[i].URI)) return &server->responses[i];
    }
    return 0;
}

static int respond_client_ssl(server_ssl_t* server, SSL* client_ssl, char* request)
{
    HTTP_request_header_t request_header = HTTP_parse_request(request);
    if(request_header.request_type == INVALID) return 1;

    response_ssl_t* response = find_response(server, request_header.URI);
    if(!response) return 1;

    return response->response_function(client_ssl, response->URI, request);
}

typedef struct {
    int clientfd;
    server_ssl_t* server;
} clientfd_server_pair_ssl_t;

static void* handle_client_ssl(void* vptr_clientfd_server_pair)
{
    clientfd_server_pair_ssl_t temp = *((clientfd_server_pair_ssl_t*) vptr_clientfd_server_pair);
    int client_sockfd = temp.clientfd;
    server_ssl_t* ptr_server = temp.server;
    free(vptr_clientfd_server_pair);

    SSL* client_ssl = SSL_new(ptr_server->ssl_context);
    if(!client_ssl){
        fprintf(stderr, "Could not create SSL for client\n");
        ERR_print_errors_fp(stderr);
        SSL_free(client_ssl);
        close(client_sockfd);
        return 0;
    }

    int res = SSL_set_fd(client_ssl, client_sockfd);
    if(res != 1){
        fprintf(stderr, "Could not set SSL file descriptor for client\n");
        ERR_print_errors_fp(stderr);
        SSL_free(client_ssl);
        close(client_sockfd);
        return 0;
    }

    res = SSL_accept(client_ssl); 
    if(res <= 0){
        fprintf(stderr, "Could not set SSL file descriptor for client\n");
        ERR_print_errors_fp(stderr);
        SSL_free(client_ssl);
        close(client_sockfd);
        return 0;
    }
    

    printf("Filedescriptor: %i\n", client_sockfd);
    char buffer[4096];

    while(1){
        int offset = 0;
        int offset_prev = 0;
        char* header_end = 0;
        bzero(buffer, sizeof(buffer));
        size_t amnt;
        while(1){
            amnt = SSL_peek(client_ssl, buffer+offset, sizeof(buffer) - offset - 1);

            if(amnt == 0){
                printf("Client disconnected!\n");
                goto exit;
            } else if(amnt < 0){
                fprintf(stderr, "Could not read client HTTP header. Error: %i!\n", SSL_get_error(client_ssl, amnt));
                goto exit;
            }

            offset += amnt;
            buffer[offset] = '\0';
            header_end = strstr(buffer, "\r\n\r\n");
            if(header_end){
                SSL_read(client_ssl, buffer + offset_prev, header_end - buffer - offset_prev + strlen("\r\n\r\n"));
                offset = (header_end - buffer) + 4;     //  points past last element ie \n
                break;
            } else {
                SSL_read(client_ssl, buffer + offset_prev, offset - offset_prev);
                offset_prev = offset;
            }
        }

        HTTP_request_header_t request_header = HTTP_parse_request(buffer);
        if(request_header.request_type == INVALID) {
            printf("Received faulty HTTP header!\n");
            free_HTTP_request_header(request_header);
            goto exit;
        }

        int content_len = 0;
        char* content_len_str = field_value(request_header.fields, request_header.fields_len, "Content-Length");
        if(content_len_str) {
            errno = 0;
            content_len = strtol(content_len_str, 0, 10);
            if(content_len == 0 && errno) {
                printf("Malformed content length!\n");
                free_HTTP_request_header(request_header);
                goto exit;
            }
        }

        
        free_HTTP_request_header(request_header);

        if(offset + content_len >= sizeof(buffer)){
            printf("Client receive buffer too small!\n");
            goto exit;
        }

        int cont_offset = 0;
        while(cont_offset < content_len){
            amnt = SSL_read(client_ssl, buffer + offset + cont_offset, content_len - cont_offset);

            if(amnt == 0){
                printf("Client disconnected!\n");
                goto exit;
            } else if(amnt < 0){
                perror("Could not read client message!\n");
                goto exit;
            }

            cont_offset += amnt;
        }
        buffer[offset + cont_offset] = '\0';

        printf("Received the following message:\n");
        printf("%s\n\n", buffer);

        if(respond_client_ssl(ptr_server, client_ssl, buffer)){
            printf("Error when responding to client\n");
        }
    }

exit:
    close(client_sockfd);
    SSL_shutdown(client_ssl);
    SSL_free(client_ssl);
    printf("Finished client handling.\n\n");
    return 0;
}

int start_server_ssl(server_ssl_t* server)
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


        clientfd_server_pair_ssl_t* data = malloc(sizeof(clientfd_server_pair_ssl_t));    // free called in new thread
        if(!data) continue;
        *data = (clientfd_server_pair_ssl_t){.clientfd = *client_sockfd, .server = server};

        handle_client_ssl(data);
    }

    return 0;
}

int start_server_ssl_multithread(server_ssl_t* server)
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
        clientfd_server_pair_ssl_t* data = malloc(sizeof(clientfd_server_pair_ssl_t));    // free called in new thread
        if(!data) continue;
        *data = (clientfd_server_pair_ssl_t){.clientfd = *client_sockfd, .server = server};

        pthread_create(&thread, 0, handle_client_ssl, data);
        pthread_detach(thread);
    }

    return 0;
}
