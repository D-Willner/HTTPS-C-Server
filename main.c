#include "server.h"
#include "server_ssl.h"
#include "test/test_http.h"

int main(int argc, char** argv)
{
    // ///*
    // server_t server = create_server(1024);
    // register_resource(&server, "/", (response_t){standard_GET_response, "/"});
    // return start_server_multithread(&server);
    // //*/

    
    server_ssl_t server_ssl = create_server_ssl(1443);
    if(server_ssl.sockfd < 0) return 1;

    register_resource_ssl(&server_ssl, "/", (response_ssl_t){standard_GET_response_ssl, "/"});
    return start_server_ssl_multithread(&server_ssl);
    


    /*
    int request_result = test_http_parse_request();
    int create_result = test_http_create();

    return request_result == EXIT_SUCCESS && create_result == EXIT_SUCCESS
        ? EXIT_SUCCESS : EXIT_FAILURE;
    */
}
