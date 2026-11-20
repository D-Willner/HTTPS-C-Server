#include "server.h"
#include "test/test_http.h"

#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <stdio.h>

int main(int argc, char** argv)
{
    //server_t server = create_server(1024);
    //return start_server_multithread(server);

    return test_http_parse_request();
}
