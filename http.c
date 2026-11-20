#include "http.h"
#include <errno.h>
#include <limits.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <stdio.h>

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
#include <stdlib.h>

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
    free(request_header.host);
    free(request_header.user_agent);
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

// Spaghetti
HTTP_request_header_t HTTP_parse_request(char* request)
{
    char* header;
    int res = HTTP_isolate_header(request, &header);
    if(res < 0){
        // error
    }

    HTTP_request_header_t HTTP_request_header = {0};

    //  Read first line:
    char* line;
    int sz = get_line(request, &line);
    printf("Line: %s\n", line);
    fflush(stdout);
    if(sz < 0){
        // error
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
        printf("Could not parse HTTP Header");
        abort();
        // not implemented
    }

    ptr = strchr(ptr, ' ') + 1;
    if(ptr - line > sz){
        // bad
    }
    end = ptr + strcspn(ptr, " \r\n")-1;
    if(!end){
        // bad
        free(line);
        abort();
        //return 0;
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
            // error
        }
        if(sz <= end - line + 1 || ((sz == end - line + 2) && *(end+1) == ' ')) {
            // error
        } 
        char* left = substr(line, end-line);

        ptr = *(end+1) == ' ' ? end+2 : end+1;
        int temp = strcspn(ptr, "\r\n");
        end = ptr + temp;

        char* right = substr(ptr, end-ptr);

        HTTP_pair_t pair = {left, right};

        if(!strcmp(left,"Host") && 
            HTTP_request_header.host == 0){
            HTTP_request_header.host = right;
            free(left);
        } else if(!strcmp(left,"User-Agent") && 
            HTTP_request_header.user_agent == 0){
            HTTP_request_header.user_agent = right;
            free(left);
        } else if(!strcmp(left,"Connection")){
            if(!strcmp(right, "Keep-Alive")) HTTP_request_header.connection = HTTP_keep_alive;
            else if(!strcmp(right, "Close")) HTTP_request_header.connection = HTTP_close;
            else HTTP_request_header.connection = HTTP_connection_ignore;
            free(left);
            free(right);
        } else if(!strcmp(left,"Cookie")){
            HTTP_request_header.cookie = atoi(right);
            free(left);
            free(right);
        } else if(!strcmp(left,"Content-Type")){
            if(!strcmp(right, "text/html; charset=utf-8")) HTTP_request_header.content_type = HTTP_text;
            else if(!strcmp(right, "application/json")) HTTP_request_header.content_type = HTTP_json;
            else HTTP_request_header.content_type = HTTP_content_ignore;
            free(left);
            free(right);
        } else if(!strcmp(left,"Content-Length")){
            HTTP_request_header.content_length = atoi(right);
            free(left);
            free(right);
        } else {
            (*(HTTP_request_header.fields + field_cnt))[0] = left;
            (*(HTTP_request_header.fields + field_cnt))[1] = right;
            field_cnt++;
            HTTP_request_header.fields_len++;
        }

        cur += sz;
        cur = skip_nl(cur);
        free(line);
    }

    free(header);
    return HTTP_request_header;
}

HTTP_response_header_t HTTP_parse_response(char* response)
{
    HTTP_response_header_t parsed = {.status = HTTP_status_ignore};
    if(!response) return parsed;

    char* header_end = strstr(response, "\r\n\r\n");
    if(!header_end) return parsed;

    // Read the status code. The reason phrase can vary.
    char* line_end = strstr(response, "\r\n");
    if(line_end - response < 12 || response + strcspn(response, "\r\n") != line_end ||
        strncmp(response, "HTTP/1.", 7) ||
        response[7] < '0' || response[7] > '9' || response[8] != ' '){
        return parsed;
    }
    char* code = response + 9;
    if(code + 3 != line_end && code[3] != ' ') return parsed;

    for(size_t i = 0; i < sizeof(HTTP_response_status_conv)/sizeof(HTTP_response_status_conv[0]); i++){
        if(!strncmp(code, HTTP_response_status_conv[i], 3)){
            parsed.status = (HTTP_response_status_t)i;
            break;
        }
    }
    if(parsed.status == HTTP_status_ignore) return parsed;
    parsed.set_cookie = HTTP_no_cookie;

    int field_amnt = 0;
    for(char* end = line_end; end < header_end; end = strstr(end + 2, "\r\n")){
        if(field_amnt == INT_MAX) goto fail;
        field_amnt++;
    }
    if(field_amnt){
        parsed.fields = calloc(field_amnt, sizeof(HTTP_pair_t));
        if(!parsed.fields) goto fail;
    }

    int have_content_length = 0;
    while(line_end < header_end){
        char* cur = line_end + 2;
        line_end = strstr(cur, "\r\n");
        if(cur + strcspn(cur, "\r\n") != line_end) goto fail;

        char* colon = cur + strspn(cur, "!#$%&'*+-.^_`|~0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz");
        if(colon == cur || *colon != ':') goto fail;

        char* value = colon + 1;
        value += strspn(value, " \t");
        char* end = line_end;
        while(end > value && (end[-1] == ' ' || end[-1] == '\t')) end--;

        char* left = substr(cur, colon - cur);
        char* right = substr(value, end - value);
        HTTP_pair_t pair = {left, right};
        if(!left || !right){
            free_HTTP_pair(pair);
            goto fail;
        }

        if(!strcasecmp(left, "Connection")){
            if(!strcasecmp(right, "Keep-Alive")) parsed.connection = HTTP_keep_alive;
            else if(!strcasecmp(right, "Close")) parsed.connection = HTTP_close;
            else parsed.connection = HTTP_connection_ignore;
        } else if(!strcasecmp(left, "Content-Type")){
            size_t len = strcspn(right, ";");
            while(len > 0 && (right[len-1] == ' ' || right[len-1] == '\t')) len--;
            if(len == strlen("text/html") && !strncasecmp(right, "text/html", len)) parsed.content_type = HTTP_text;
            else if(len == strlen("application/json") && !strncasecmp(right, "application/json", len)) parsed.content_type = HTTP_json;
            else parsed.content_type = HTTP_content_ignore;
        } else if(!strcasecmp(left, "Content-Length")){
            char* tail;
            errno = 0;
            long length = strtol(right, &tail, 10);
            if(*right < '0' || *right > '9' || *tail || errno == ERANGE || length > INT_MAX ||
                (have_content_length && parsed.content_length != length)){
                free_HTTP_pair(pair);
                goto fail;
            }
            parsed.content_length = (int)length;
            have_content_length = 1;
        } else {
            if(!strcasecmp(left, "Set-Cookie")) parsed.set_cookie = HTTP_set_cookie;
            parsed.fields[parsed.fields_len][0] = left;
            parsed.fields[parsed.fields_len][1] = right;
            parsed.fields_len++;
            continue;
        }
        free_HTTP_pair(pair);
    }

    return parsed;

fail:
    free_HTTP_response_header(parsed);
    return (HTTP_response_header_t){.status = HTTP_status_ignore};
}

char* HTTP_create_request(const HTTP_request_header_t request);
char* HTTP_create_response(const HTTP_response_header_t response);

int HTTP_write_request(const HTTP_request_header_t request, char* buffer);
int HTTP_write_response(const HTTP_response_header_t response, char* buffer);
