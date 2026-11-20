#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "test_http.h"

int main(int argc, char **argv)
{
    if (argc == 2) {
        if (strcmp(argv[1], "create") == 0) return test_http_create();
        return test_http_parse_request_case(argv[1]);
    }
    if (argc == 3 && strcmp(argv[1], "response") == 0) {
        return test_http_parse_response_case(argv[2]);
    }
    if (argc == 3 && strcmp(argv[1], "create") == 0) {
        return test_http_create_case(argv[2]);
    }
    if (argc != 1) {
        fprintf(stderr, "Usage: %s [request_case | response response_case | create [case]]\n",
                argv[0]);
        return EXIT_FAILURE;
    }

    int create_result = test_http_create();
    int request_result = test_http_parse_request();
    int response_result = test_http_parse_response();
    return create_result == EXIT_SUCCESS && request_result == EXIT_SUCCESS &&
           response_result == EXIT_SUCCESS
        ? EXIT_SUCCESS : EXIT_FAILURE;
}
