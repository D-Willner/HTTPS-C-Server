#include "test_http.h"

int main(int argc, char **argv)
{
    if (argc == 2) return test_http_parse_request_case(argv[1]);
    return test_http_parse_request();
}
