#pragma once

typedef enum {
    HTTP_Request,
    HTTP_Response
} HTTP_type_t;

typedef enum {
    INVALID,
    GET,
    POST
} HTTP_request_type_t;

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
    char* URI;
    HTTP_pair_t* fields;
    int fields_len;
} HTTP_request_header_t;

typedef struct {
    HTTP_response_status_t status;
    HTTP_pair_t* fields;
    int fields_len;
} HTTP_response_header_t;

//  Returns index of field if it exists. Otherwise -1.
int has_field(HTTP_pair_t* fields, int field_amnt, char* fieldname);
char* field_value(HTTP_pair_t* fields, int field_amnt, char* fieldname);

int request_has_field(HTTP_request_header_t request_header, char* fieldname);
char* request_field_value(HTTP_request_header_t request_header, char* fieldname);

int response_has_field(HTTP_response_header_t response_header, char* fieldname);
char* response_field_value(HTTP_response_header_t response_header, char* fieldname);

void free_HTTP_request_header(HTTP_request_header_t request_header);
void free_HTTP_response_header(HTTP_response_header_t response_header);

HTTP_request_header_t HTTP_parse_request(char* request);
/* Parses a complete HTTP/1.x header. HTTP_status_ignore signals failure. */
HTTP_response_header_t HTTP_parse_response(char* response);

char* HTTP_create_request(const HTTP_request_header_t request);
char* HTTP_create_response(const HTTP_response_header_t response);
