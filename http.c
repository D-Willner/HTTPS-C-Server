#include "http.h"
#include <errno.h>
#include <limits.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <stdio.h>

char* HTTP_response_status_conv[] = {
    [HTTP_status_ignore] = 0,
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

char* HTTP_response_status_code[] = {
    [HTTP_status_ignore] = 0,
    [Continue_100] = "100",
    [OK_200] = "200",
    [Created_201] = "201",
    [NoContent_204] = "204",
    [Moved_301] = "301",
    [Found_302] = "302",
    [NotMod_304] = "304",
    [BadReq_400] = "400",
    [Unauth_401] = "401",
    [Forbidden_403] = "403",
    [NotFound_404] = "404",
    [TooManyReq_429] = "429",
    [IntServerErr_500] = "500",
    [BadGateway_502] = "502",
    [ServUnav_503] = "503"
};


int has_field(HTTP_pair_t* fields, int field_amnt, char* fieldname)
{
    for(int i = 0; i < field_amnt; i++){
        if(!strcmp(fields[i][0], fieldname)) return i;
    }

    return -1;
}

char* field_value(HTTP_pair_t* fields, int field_amnt, char* fieldname)
{
    int index  = has_field(fields, field_amnt, fieldname);
    if(index < 0) return 0;

    return fields[index][1];
}

int request_has_field(HTTP_request_header_t request_header, char* fieldname)
{
    return has_field(request_header.fields, request_header.fields_len, fieldname);
}

char* request_field_value(HTTP_request_header_t request_header, char* fieldname)
{
    return field_value(request_header.fields, request_header.fields_len, fieldname);
}

int response_has_field(HTTP_response_header_t response_header, char* fieldname)
{
    return has_field(response_header.fields, response_header.fields_len, fieldname);
}

char* response_field_value(HTTP_response_header_t response_header, char* fieldname)
{
    return field_value(response_header.fields, response_header.fields_len, fieldname);
}


static char* substr(const char* source, size_t n)
{
    if(!source) return 0;

    char* ret = malloc(sizeof(char)*(n+1));
    if(!ret) return 0;
    memcpy(ret, source, n);
    ret[n] = '\0';

    return ret;
}

static char* skip_nl(char* source)
{
    char* ret = source;
    for(int i = 0; i < strlen(source); i++){
        if((*ret == '\r') || (*ret == '\n')) ret++;
    }

    return ret;
}

static int HTTP_isolate_header(char* buffer, char** out_header)
{
    char* end = strstr(buffer, "\r\n\r\n");
    if(!end) {
        *out_header = malloc(strlen(buffer)+5);
        strcpy(*out_header, buffer);
    } else{
        *out_header = substr(buffer, end+4-buffer);
    }

    return strlen(*out_header);
}

static int HTTP_header_lines(char* header)
{
    if(!header) return -1;
    int len = strlen(header);

    int sum = 0;
    char* cur = header;
    while(1){
        cur = strchr(cur, '\n');
        sum++;
        if(cur == 0 || cur - header >= len - 4) break;
        cur++;
    }

    return sum;
}

static int get_line(char* str, char** out)
{
    char* line_end = strchr(str, '\r');
    size_t sz;
    if(!line_end) sz = strlen(str);
    else sz = line_end - str;

    *out = substr(str, sz);

    return sz;
}

void free_HTTP_pair(HTTP_pair_t pair)
{
    free(pair[0]);
    free(pair[1]);
}

void free_HTTP_request_header(HTTP_request_header_t request_header)
{
    free(request_header.URI);
    for(int i = 0; i < request_header.fields_len; i++){
        free_HTTP_pair(request_header.fields[i]);
    }
    free(request_header.fields);
}

void free_HTTP_response_header(HTTP_response_header_t response_header)
{
    for(int i = 0; i < response_header.fields_len; i++){
        free_HTTP_pair(response_header.fields[i]);
    }
    free(response_header.fields);
}

HTTP_request_header_t HTTP_parse_request(char* request)
{
    char* header;
    int res = HTTP_isolate_header(request, &header);
    if(res < 0){
        goto fail;
    }

    HTTP_request_header_t HTTP_request_header = {0};

    //  Read first line:
    char* line;
    int sz = get_line(header, &line);
    if(sz < 0){
        goto fail;
    }

    char* cur = header;
    char* ptr;
    char* end;

    char* ptrget = strstr(line,"GET");
    char* ptrpost = strstr(line,"POST");

    if(ptrget && ptrpost){
        if(ptrget < ptrpost){
            HTTP_request_header.request_type = GET;
            ptr = ptrget;
        } else {
            HTTP_request_header.request_type = POST;
            ptr = ptrpost;
        }
    } else if(ptrget){
        HTTP_request_header.request_type = GET;
        ptr = ptrget;
    } else if(ptrpost){
        HTTP_request_header.request_type = POST;
        ptr = ptrpost;
    } else{
        goto fail;
    }

    ptr = strchr(ptr, ' ') + 1;
    if(ptr - line > sz){
        goto fail;
    }
    end = ptr + strcspn(ptr, " \r\n")-1;
    if(!end){
        goto fail;
    }
    char* URI = malloc(end - ptr + 2);
    memcpy(URI, ptr, end-ptr+1);
    URI[end-ptr+1] = '\0';

    HTTP_request_header.URI = URI;

    cur += sz;
    cur = skip_nl(cur);
    free(line);

    int field_amnt = HTTP_header_lines(header)-1;
    HTTP_request_header.fields = malloc(sizeof(HTTP_pair_t) * field_amnt);

    int field_cnt = 0;
    while(cur - header <= strlen(header)-4){
        sz = get_line(cur, &line);
        if(!(end = strchr(line, ':'))){
            goto fail;
        }
        if(sz <= end - line + 1 || ((sz == end - line + 2) && *(end+1) == ' ')) {
            goto fail;
        } 
        char* left = substr(line, end-line);

        ptr = *(end+1) == ' ' ? end+2 : end+1;
        int temp = strcspn(ptr, "\r\n");
        end = ptr + temp;

        char* right = substr(ptr, end-ptr);

        HTTP_pair_t pair = {left, right};

        (*(HTTP_request_header.fields + field_cnt))[0] = left;
        (*(HTTP_request_header.fields + field_cnt))[1] = right;
        field_cnt++;
        HTTP_request_header.fields_len++;

        cur += sz;
        cur = skip_nl(cur);
        free(line);
    }

    free(header);
    return HTTP_request_header;

fail:
    free(header);
    free(line);
    HTTP_request_header_t ret = {0};
    return ret;
}

int HTTP_code_ind(char* code_beg)
{
    for(int i = 0; i < sizeof(HTTP_response_status_code)/sizeof(HTTP_response_status_code[0]); i++){
        if(HTTP_response_status_code[i] && !memcmp(code_beg, HTTP_response_status_code[i], 3)){
            return i;
        }
    }

    return -1;
}

HTTP_response_header_t HTTP_parse_response(char* response)
{
    char* header;
    int res = HTTP_isolate_header(response, &header);
    if(res < 0){
        goto fail;
    }

    HTTP_response_header_t HTTP_response_header = {0};

    //  Read first line:
    char* line;
    int sz = get_line(header, &line);
    if(sz < 0){
        goto fail;
    }

    char* cur = strstr(line, "HTTP/");
    char* ptr;
    char* end;

    if(!cur){
        goto fail;
    } else if (cur == line){    // Format: HTTP/1.1 200 OK
        cur = strchr(line, ' ');
        if(!cur) goto fail;
        cur++;
    } else {                    // Format: 200 OK HTTP/1.1
        cur = line;
    }
    int code_index = HTTP_code_ind(cur);
    if(code_index < 0) goto fail;
    HTTP_response_header.status = code_index;   // works

    cur = header + sz;
    cur = skip_nl(cur);
    free(line);

    int field_amnt = HTTP_header_lines(header)-1;
    HTTP_response_header.fields = malloc(sizeof(HTTP_pair_t) * field_amnt);

    int field_cnt = 0;
    while(cur - header <= strlen(header)-4){
        sz = get_line(cur, &line);
        if(!(end = strchr(line, ':'))){
            goto fail;
        }
        if(sz <= end - line + 1 || ((sz == end - line + 2) && *(end+1) == ' ')) {
            goto fail;
        } 
        char* left = substr(line, end-line);

        ptr = *(end+1) == ' ' ? end+2 : end+1;
        int temp = strcspn(ptr, "\r\n");
        end = ptr + temp;

        char* right = substr(ptr, end-ptr);

        HTTP_pair_t pair = {left, right};

        (*(HTTP_response_header.fields + field_cnt))[0] = left;
        (*(HTTP_response_header.fields + field_cnt))[1] = right;
        field_cnt++;
        HTTP_response_header.fields_len++;

        cur += sz;
        cur = skip_nl(cur);
        free(line);
    }

    free(header);
    return HTTP_response_header;

fail:
    free(header);
    free(line);
    HTTP_response_header_t ret = {0};
    return ret;
}

size_t request_buffer_sz(const HTTP_request_header_t request)
{
    size_t sz = 0;
    sz += request.request_type == GET ? strlen("GET ") : strlen("POST ");
    sz += strlen(request.URI);
    sz += strlen(" ");
    sz += strlen("HTTP/1.1\r\n");

    for(int i = 0; i < request.fields_len; i++){
        sz += strlen(((request.fields[i]))[0]);
        sz += strlen(": ");
        sz += strlen(((request.fields[i]))[1]);
        sz += strlen("\r\n");
    }

    sz += strlen("\r\n"); // empty line at end

    return sz;
}

char* HTTP_create_request(const HTTP_request_header_t request)
{
    size_t size = request_buffer_sz(request);
    char* buffer = malloc(size + 1);
    if(!buffer) return NULL;
    size_t cur = 0;

    request.request_type == GET ? memcpy(buffer,  "GET ", strlen("GET "))
        : memcpy(buffer,  "POST ", strlen("POST "));
    cur += request.request_type == GET ? strlen("GET ") : strlen("POST ");

    memcpy(buffer+cur, request.URI, strlen(request.URI));
    cur+= strlen(request.URI);
    memcpy(buffer+cur, " ", strlen(" "));
    cur+= strlen(" ");
    memcpy(buffer+cur, "HTTP/1.1\r\n", strlen("HTTP/1.1\r\n"));
    cur+= strlen("HTTP/1.1\r\n");

    for(int i = 0; i < request.fields_len; i++){
        memcpy(buffer+cur, ((request.fields[i]))[0], strlen(((request.fields[i]))[0]));
        cur += strlen(((request.fields[i]))[0]);
        memcpy(buffer+cur, ": ", strlen(": "));
        cur += strlen(": ");
        memcpy(buffer+cur, ((request.fields[i]))[1], strlen(((request.fields[i]))[1]));
        cur += strlen(((request.fields[i]))[1]);
        memcpy(buffer+cur, "\r\n", strlen("\r\n"));
        cur += strlen("\r\n");
    }

    memcpy(buffer+cur, "\r\n", strlen("\r\n"));
    cur += strlen("\r\n"); // empty line at end

    if(cur != size){
        //failed
        free(buffer);
        return 0;
    }

    buffer[cur] = '\0';
    return buffer;
}

size_t response_buffer_sz(const HTTP_response_header_t response)
{
    size_t sz = 0;
    sz += strlen("HTTP/1.1 ");
    sz += strlen(HTTP_response_status_conv[response.status]);
    sz += strlen("\r\n");

    for(int i = 0; i < response.fields_len; i++){
        sz += strlen(((response.fields[i]))[0]);
        sz += strlen(": ");
        sz += strlen(((response.fields[i]))[1]);
        sz += strlen("\r\n");
    }

    sz += strlen("\r\n"); // empty line at end

    return sz;
}

char* HTTP_create_response(const HTTP_response_header_t response)
{
    size_t size = response_buffer_sz(response);
    char* buffer = malloc(size + 1);
    if(!buffer) return NULL;
    size_t cur = 0;

    memcpy(buffer, "HTTP/1.1 ", strlen("HTTP/1.1 "));
    cur += strlen("HTTP/1.1 ");
    memcpy(buffer+cur, HTTP_response_status_conv[response.status], strlen(HTTP_response_status_conv[response.status]));
    cur += strlen(HTTP_response_status_conv[response.status]);
    memcpy(buffer+cur, "\r\n", strlen("\r\n"));
    cur += strlen("\r\n");

    for(int i = 0; i < response.fields_len; i++){
        memcpy(buffer+cur, ((response.fields[i]))[0], strlen(((response.fields[i]))[0]));
        cur += strlen(((response.fields[i]))[0]);
        memcpy(buffer+cur, ": ", strlen(": "));
        cur += strlen(": ");
        memcpy(buffer+cur, ((response.fields[i]))[1], strlen(((response.fields[i]))[1]));
        cur += strlen(((response.fields[i]))[1]);
        memcpy(buffer+cur, "\r\n", strlen("\r\n"));
        cur += strlen("\r\n");
    }

    memcpy(buffer+cur, "\r\n", strlen("\r\n"));
    cur += strlen("\r\n"); // empty line at end

    if(cur != size){
        //failed
        free(buffer);
        return 0;
    }

    buffer[cur] = '\0';
    return buffer;
}
