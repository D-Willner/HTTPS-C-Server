#pragma once

typedef enum {
    HTTP_Request,
    HTTP_Response
} HTTP_type_t;

typedef enum {
    GET,
    POST
} HTTP_request_type_t;

typedef enum {
    HTTP_content_ignore,
    HTTP_text,
    HTTP_json
} HTTP_content_type_t;

typedef enum {
    HTTP_connection_ignore,
    HTTP_keep_alive,
    HTTP_close
} HTTP_connection_type_t;

typedef enum {
    HTTP_cookie_ignore,
    HTTP_set_cookie,
    HTTP_no_cookie
} HTTP_cookie_instr_t;

typedef char* HTTP_pair_t[2];   //  'fieldname' : 'fieldvalue'

typedef enum {
    HTTP_status_ignore = 0,
    Continue_100,
    OK_200,
    Created_201,
    NoContent_204,
    Moved_301,
    Found_302,
    NotMod_304,
    BadReq_400,
    Unauth_401,
    Forbidden_403,
    NotFound_404,
    TooManyReq_429,
    IntServerErr_500,
    BadGateway_502,
    ServUnav_503
} HTTP_response_status_t;

extern char* HTTP_response_status_conv[];

typedef struct {
    HTTP_request_type_t request_type;
    char* host;
    char* user_agent;
    char* URI;
    HTTP_connection_type_t connection;
    HTTP_content_type_t content_type;
    int content_length;
    unsigned long cookie;
    HTTP_pair_t* fields;
    int fields_len;
} HTTP_request_header_t;

typedef struct {
    HTTP_response_status_t status;
    HTTP_connection_type_t connection;
    HTTP_content_type_t content_type;
    int content_length;
    HTTP_cookie_instr_t set_cookie;
    HTTP_pair_t* fields;
    int fields_len;
} HTTP_response_header_t;

void free_HTTP_request_header(HTTP_request_header_t request_header);
void free_HTTP_response_header(HTTP_response_header_t response_header);

HTTP_request_header_t HTTP_parse_request(char* request);
/* Parses a complete HTTP/1.x header. HTTP_status_ignore signals failure. */
HTTP_response_header_t HTTP_parse_response(char* response);

char* HTTP_create_request(const HTTP_request_header_t request);
char* HTTP_create_response(const HTTP_response_header_t response);

int HTTP_write_request(const HTTP_request_header_t request, char* buffer);
int HTTP_write_response(const HTTP_response_header_t response, char* buffer);
