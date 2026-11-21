#include "server.h"
#include "test/test_http.h"

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <stdio.h>

int main(int argc, char** argv)
{
    server_t server = create_server(1024);
    register_resource(&server, "/", (response_t){standard_GET_response, "/"});
    return start_server_multithread(&server);

    /*
    int request_result = test_http_parse_request();
    int create_result = test_http_create();

    return request_result == EXIT_SUCCESS && create_result == EXIT_SUCCESS
        ? EXIT_SUCCESS : EXIT_FAILURE;
    */
}
