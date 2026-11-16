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
    HTTP_text,
    HTTP_json,
    HTTP_content_ignore
} HTTP_content_type_t;

typedef enum {
    HTTP_keep_alive,
    HTTP_close,
    HTTP_connection_ignore
} HTTP_connection_type_t;

typedef enum {
    HTTP_set_cookie,
    HTTP_no_cookie,
    HTTP_cookie_ignore
} HTTP_cookie_instr_t;

typedef char* HTTP_pair_t[2];   //  'fieldname' : 'fieldvalue'

typedef enum {
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

char* HTTP_response_status_conv[] = {
    [Continue_100] = "100 Continue",
    [OK_200] = "200 OK",
    [Created_201] = "201 Created",
    [NoContent_204] = "204 No Content",
    [Moved_301] = "301 Moved Permanently",
    [Found_302] = "302 Found",
    [NotMod_304] = "304 Not Modified",
    [BadReq_400] = "400 Bad Request",
    [Unauth_401] = "401 Unauthorized",
    [Forbidden_403] = "403 Forbidden",
    [NotFound_404] = "404 Not Found",
    [TooManyReq_429] = "429 Too Many Requests",
    [IntServerErr_500] = "500 Internal Server Error",
    [BadGateway_502] = "502 Bad Gateway",
    [ServUnav_503] = "503 Service Unavailable"
};

typedef struct {
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

HTTP_request_header_t HTTP_parse_request(char* request);
HTTP_response_header_t HTTP_parse_response(char* response);

char* HTTP_create_request(const HTTP_request_header_t request);
char* HTTP_create_response(const HTTP_response_header_t response);

int HTTP_write_request(const HTTP_request_header_t request, char* buffer);
int HTTP_write_response(const HTTP_response_header_t response, char* buffer);
