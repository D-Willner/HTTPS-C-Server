#include "http.h"
#include <string.h>

int get_line(char* str, char* out)
{
    char* line_end = strchr(str, '\n');
    size_t sz = line_end - str + 1;

    char* line = malloc(sz+1);
    if(!line){
        //  gifted too much ram to sam :(
        return -1;
    }
    memcpy(line, line_end, sz);
    line[sz] = '\0';

    line_end = line_end + sz;

    out = line;

    return sz;
}

HTTP_request_header_t HTTP_parse_request(char* request)
{
    //  Read first line:
    char* line;
    int sz = get_line(request, line);
    int c = 0;
    if(sz < 0){
        // error
    }

    //do stuff

    c += sz;
    free(line);

    while(1){
        sz = get_line(request + c, line);

        c += sz;
        free(line);
    }
}

HTTP_response_header_t HTTP_parse_response(char* response);

char* HTTP_create_request(const HTTP_request_header_t request);
char* HTTP_create_response(const HTTP_response_header_t response);

int HTTP_write_request(const HTTP_request_header_t request, char* buffer);
int HTTP_write_response(const HTTP_response_header_t response, char* buffer);