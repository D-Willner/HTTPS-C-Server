#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../http.h"
#include "test_http.h"

typedef struct {
    const char *name;
    const char *request;
    HTTP_request_header_t expected;
    const char *field_names[2];
    const char *field_values[2];
} parse_case_t;

/* Exercise only the parser's GET/POST and named-header subset of HTTP. */
static const parse_case_t cases[] = {
    {
        .name = "get_defaults",
        .request = "GET / HTTP/1.1\r\n\r\n",
        .expected = {.request_type = GET, .URI = "/"},
    },
    {
        .name = "get_uri",
        .request = "GET /resources/index.html?name=test&count=2 HTTP/1.1\r\n\r\n",
        .expected = {.request_type = GET,
                     .URI = "/resources/index.html?name=test&count=2"},
    },
    {
        .name = "post_json",
        .request = "POST /api HTTP/1.1\r\nContent-Type: application/json\r\n"
                   "Content-Length: 2\r\n\r\n{}",
        .expected = {.request_type = POST, .URI = "/api",
                     .content_type = HTTP_json, .content_length = 2},
    },
    {
        .name = "host",
        .request = "GET / HTTP/1.1\r\nHost: localhost:8080\r\n\r\n",
        .expected = {.URI = "/", .host = "localhost:8080"},
    },
    {
        .name = "user_agent",
        .request = "GET / HTTP/1.1\r\nUser-Agent: Test Client/1.0\r\n\r\n",
        .expected = {.URI = "/", .user_agent = "Test Client/1.0"},
    },
    {
        .name = "keep_alive",
        .request = "GET / HTTP/1.1\r\nConnection: Keep-Alive\r\n\r\n",
        .expected = {.URI = "/", .connection = HTTP_keep_alive},
    },
    {
        .name = "close",
        .request = "GET / HTTP/1.1\r\nConnection: Close\r\n\r\n",
        .expected = {.URI = "/", .connection = HTTP_close},
    },
    {
        .name = "numeric_cookie",
        .request = "GET / HTTP/1.1\r\nCookie: 123456\r\n\r\n",
        .expected = {.URI = "/", .cookie = 123456},
    },
    {
        .name = "post_text",
        .request = "POST /submit HTTP/1.1\r\n"
                   "Content-Type: text/html; charset=utf-8\r\n"
                   "Content-Length: 5\r\n\r\nhello",
        .expected = {.request_type = POST, .URI = "/submit",
                     .content_type = HTTP_text, .content_length = 5},
    },
    {
        .name = "extra_fields",
        .request = "GET / HTTP/1.1\r\nX-First: alpha\r\nX-Second: beta gamma\r\n\r\n",
        .expected = {.URI = "/", .fields_len = 2},
        .field_names = {"X-First", "X-Second"},
        .field_values = {"alpha", "beta gamma"},
    },
    {
        .name = "no_space_after_colon",
        .request = "GET / HTTP/1.1\r\nContent-Length:0\r\nX-Test:value\r\n\r\n",
        .expected = {.URI = "/", .fields_len = 1},
        .field_names = {"X-Test"},
        .field_values = {"value"},
    },
    {
        .name = "unrecognized_values",
        .request = "POST / HTTP/1.1\r\nConnection: Close\r\n"
                   "Content-Type: application/octet-stream\r\n\r\n",
        .expected = {.request_type = POST, .URI = "/",
                     .connection = HTTP_close, .content_type = HTTP_content_ignore},
    },
    {
        .name = "unrecognized_connection",
        .request = "GET / HTTP/1.1\r\nConnection: custom\r\n\r\n",
        .expected = {.URI = "/", .connection = HTTP_connection_ignore},
    },
    {
        .name = "body_is_not_headers",
        .request = "POST / HTTP/1.1\r\nContent-Length: 14\r\n\r\nCookie: 999\r\n\r\n",
        .expected = {.request_type = POST, .URI = "/", .content_length = 14},
    },
};

static int check_string(const char *field, const char *actual, const char *expected)
{
    if (actual == NULL && expected == NULL) return 0;
    if (actual != NULL && expected != NULL && strcmp(actual, expected) == 0) return 0;
    fprintf(stderr, "%s: expected '%s', got '%s'\n", field,
            expected ? expected : "(null)", actual ? actual : "(null)");
    return 1;
}

static int run_case(const parse_case_t *test)
{
    /* The API accepts mutable input; keep the fixture itself unchanged. */
    char *request = malloc(strlen(test->request) + 1);
    if (request == NULL) return EXIT_FAILURE;
    strcpy(request, test->request);
    HTTP_request_header_t actual = HTTP_parse_request(request);
    const HTTP_request_header_t *expected = &test->expected;
    int failures = 0;

#define CHECK_NUMBER(field) do { \
    if (actual.field != expected->field) { \
        fprintf(stderr, #field ": expected %lu, got %lu\n", \
                (unsigned long)expected->field, (unsigned long)actual.field); \
        failures++; \
    } \
} while (0)

    CHECK_NUMBER(request_type);
    CHECK_NUMBER(connection);
    CHECK_NUMBER(content_type);
    CHECK_NUMBER(content_length);
    CHECK_NUMBER(cookie);
    CHECK_NUMBER(fields_len);
#undef CHECK_NUMBER

    failures += check_string("URI", actual.URI, expected->URI);
    failures += check_string("host", actual.host, expected->host);
    failures += check_string("user_agent", actual.user_agent, expected->user_agent);
    if (expected->fields_len > 0 && actual.fields == NULL) {
        fprintf(stderr, "Expected extra header fields, got NULL\n");
        failures++;
    } else if (actual.fields_len == expected->fields_len) {
        for (int i = 0; i < expected->fields_len; i++) {
            failures += check_string("field name", actual.fields[i][0], test->field_names[i]);
            failures += check_string("field value", actual.fields[i][1], test->field_values[i]);
        }
    }

    /* Parsed strings are allocated by the parser. */
    free(actual.URI);
    free(actual.host);
    free(actual.user_agent);
    for (int i = 0; actual.fields != NULL && i < actual.fields_len; i++) {
        free(actual.fields[i][0]);
        free(actual.fields[i][1]);
    }
    free(actual.fields);
    free(request);
    return failures ? EXIT_FAILURE : EXIT_SUCCESS;
}

int test_http_parse_request_case(const char *name)
{
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        if (strcmp(name, cases[i].name) == 0) {
            fprintf(stderr, "Running %s\n", name);
            int result = run_case(&cases[i]);
            fprintf(stderr, "%s: %s\n", name, result == EXIT_SUCCESS ? "PASS" : "FAIL");
            return result;
        }
    }
    fprintf(stderr, "Unknown test case: %s\n", name);
    return EXIT_FAILURE;
}

int test_http_parse_request(void)
{
    int failures = 0;
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        failures += test_http_parse_request_case(cases[i].name) != EXIT_SUCCESS;
    }
    fprintf(stderr, "%zu cases, %d failed\n", sizeof(cases) / sizeof(cases[0]), failures);
    return failures ? EXIT_FAILURE : EXIT_SUCCESS;
}
