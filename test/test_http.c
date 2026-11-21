#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../http.h"
#include "test_http.h"

typedef const char *expected_field_t[2];

typedef struct {
    const char *name;
    const char *request;
    HTTP_request_header_t expected;
    expected_field_t fields[2];
} request_case_t;

typedef struct {
    const char *name;
    const char *response;
    HTTP_response_header_t expected;
    expected_field_t fields[5];
} response_case_t;

typedef struct {
    const char *name;
    HTTP_type_t type;
    union {
        HTTP_request_header_t request;
        HTTP_response_header_t response;
    } header;
    const char *expected;
} create_case_t;

static const request_case_t request_cases[] = {
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
        .expected = {.request_type = POST, .URI = "/api", .fields_len = 2},
        .fields = {{"Content-Type", "application/json"}, {"Content-Length", "2"}},
    },
    {
        .name = "host",
        .request = "GET / HTTP/1.1\r\nHost: localhost:8080\r\n\r\n",
        .expected = {.request_type = GET, .URI = "/", .fields_len = 1},
        .fields = {{"Host", "localhost:8080"}},
    },
    {
        .name = "user_agent",
        .request = "GET / HTTP/1.1\r\nUser-Agent: Test Client/1.0\r\n\r\n",
        .expected = {.request_type = GET, .URI = "/", .fields_len = 1},
        .fields = {{"User-Agent", "Test Client/1.0"}},
    },
    {
        .name = "keep_alive",
        .request = "GET / HTTP/1.1\r\nConnection: Keep-Alive\r\n\r\n",
        .expected = {.request_type = GET, .URI = "/", .fields_len = 1},
        .fields = {{"Connection", "Keep-Alive"}},
    },
    {
        .name = "close",
        .request = "GET / HTTP/1.1\r\nConnection: Close\r\n\r\n",
        .expected = {.request_type = GET, .URI = "/", .fields_len = 1},
        .fields = {{"Connection", "Close"}},
    },
    {
        .name = "numeric_cookie",
        .request = "GET / HTTP/1.1\r\nCookie: 123456\r\n\r\n",
        .expected = {.request_type = GET, .URI = "/", .fields_len = 1},
        .fields = {{"Cookie", "123456"}},
    },
    {
        .name = "post_text",
        .request = "POST /submit HTTP/1.1\r\n"
                   "Content-Type: text/html; charset=utf-8\r\n"
                   "Content-Length: 5\r\n\r\nhello",
        .expected = {.request_type = POST, .URI = "/submit", .fields_len = 2},
        .fields = {{"Content-Type", "text/html; charset=utf-8"}, {"Content-Length", "5"}},
    },
    {
        .name = "extra_fields",
        .request = "GET / HTTP/1.1\r\nX-First: alpha\r\nX-Second: beta gamma\r\n\r\n",
        .expected = {.request_type = GET, .URI = "/", .fields_len = 2},
        .fields = {{"X-First", "alpha"}, {"X-Second", "beta gamma"}},
    },
    {
        .name = "no_space_after_colon",
        .request = "GET / HTTP/1.1\r\nContent-Length:0\r\nX-Test:value\r\n\r\n",
        .expected = {.request_type = GET, .URI = "/", .fields_len = 2},
        .fields = {{"Content-Length", "0"}, {"X-Test", "value"}},
    },
    {
        .name = "unrecognized_values",
        .request = "POST / HTTP/1.1\r\nConnection: Close\r\n"
                   "Content-Type: application/octet-stream\r\n\r\n",
        .expected = {.request_type = POST, .URI = "/", .fields_len = 2},
        .fields = {{"Connection", "Close"}, {"Content-Type", "application/octet-stream"}},
    },
    {
        .name = "unrecognized_connection",
        .request = "GET / HTTP/1.1\r\nConnection: custom\r\n\r\n",
        .expected = {.request_type = GET, .URI = "/", .fields_len = 1},
        .fields = {{"Connection", "custom"}},
    },
    {
        .name = "body_is_not_headers",
        .request = "POST / HTTP/1.1\r\nContent-Length: 15\r\n\r\nCookie: 999\r\n\r\n",
        .expected = {.request_type = POST, .URI = "/", .fields_len = 1},
        .fields = {{"Content-Length", "15"}},
    },
};

static const response_case_t response_cases[] = {
    {
        .name = "no_content",
        .response = "HTTP/1.1 204 No Content\r\n\r\n",
        .expected = {.status = NoContent_204},
    },
    {
        .name = "ok_fields",
        .response = "HTTP/1.1 200 OK\r\nConnection: Close\r\nX-Request-ID: abc\r\n"
                    "Content-Type: text/html; charset=UTF-8\r\nContent-Length:14\r\n"
                    "Set-Cookie: id=123; Path=/\r\n\r\nX-Body: data\r\n",
        .expected = {.status = OK_200, .fields_len = 5},
        .fields = {{"Connection", "Close"}, {"X-Request-ID", "abc"},
                   {"Content-Type", "text/html; charset=UTF-8"}, {"Content-Length", "14"},
                   {"Set-Cookie", "id=123; Path=/"}},
    },
    {
        .name = "repeated_fields",
        .response = "HTTP/1.1 201 Created\r\nSet-Cookie: id=123\r\n"
                    "Set-Cookie: theme=dark\r\n\r\n",
        .expected = {.status = Created_201, .fields_len = 2},
        .fields = {{"Set-Cookie", "id=123"}, {"Set-Cookie", "theme=dark"}},
    },
    {
        .name = "version_last",
        .response = "404 Not Found HTTP/1.1\r\nContent-Length: 0\r\n\r\n",
        .expected = {.status = NotFound_404, .fields_len = 1},
        .fields = {{"Content-Length", "0"}},
    },
};

static const create_case_t create_cases[] = {
    {
        .name = "get_without_fields",
        .type = HTTP_Request,
        .header.request = {.request_type = GET, .URI = "/"},
        .expected = "GET / HTTP/1.1\r\n\r\n",
    },
    {
        .name = "post_with_fields",
        .type = HTTP_Request,
        .header.request = {
            .request_type = POST,
            .URI = "/api/items?name=alpha%20beta&limit=2",
            .fields = (HTTP_pair_t[]){{"Host", "localhost:8080"},
                                      {"Content-Type", "application/json"},
                                      {"Content-Length", "0"}},
            .fields_len = 3,
        },
        .expected = "POST /api/items?name=alpha%20beta&limit=2 HTTP/1.1\r\n"
                    "Host: localhost:8080\r\nContent-Type: application/json\r\n"
                    "Content-Length: 0\r\n\r\n",
    },
    {
        .name = "no_content_without_fields",
        .type = HTTP_Response,
        .header.response = {.status = NoContent_204},
        .expected = "HTTP/1.1 204 No Content\r\n\r\n",
    },
    {
        .name = "not_found_with_empty_value",
        .type = HTTP_Response,
        .header.response = {
            .status = NotFound_404,
            .fields = (HTTP_pair_t[]){{"X-Empty", ""}, {"Content-Length", "0"}},
            .fields_len = 2,
        },
        .expected = "HTTP/1.1 404 Not Found\r\nX-Empty: \r\nContent-Length: 0\r\n\r\n",
    },
    {
        .name = "created_with_repeated_fields",
        .type = HTTP_Response,
        .header.response = {
            .status = Created_201,
            .fields = (HTTP_pair_t[]){{"Set-Cookie", "id=123; Path=/"},
                                      {"Location", "/api/items/123"},
                                      {"Set-Cookie", "theme=dark; Path=/"}},
            .fields_len = 3,
        },
        .expected = "HTTP/1.1 201 Created\r\nSet-Cookie: id=123; Path=/\r\n"
                    "Location: /api/items/123\r\nSet-Cookie: theme=dark; Path=/\r\n\r\n",
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

static int check_number(const char *field, int actual, int expected)
{
    if (actual == expected) return 0;
    fprintf(stderr, "%s: expected %d, got %d\n", field, expected, actual);
    return 1;
}

static int check_fields(HTTP_pair_t *actual, int actual_len,
                        const expected_field_t *expected, int expected_len)
{
    if (check_number("fields_len", actual_len, expected_len)) return 1;
    if (expected_len > 0 && actual == NULL) {
        fprintf(stderr, "Expected header fields, got NULL\n");
        return 1;
    }

    int failures = 0;
    for (int i = 0; i < expected_len; i++) {
        failures += check_string("field name", actual[i][0], expected[i][0]);
        failures += check_string(expected[i][0], actual[i][1], expected[i][1]);
    }
    return failures;
}

static int run_request_case(const request_case_t *test)
{
    /* The API accepts mutable input; keep the fixture itself unchanged. */
    char *request = malloc(strlen(test->request) + 1);
    if (request == NULL) return EXIT_FAILURE;
    strcpy(request, test->request);
    HTTP_request_header_t actual = HTTP_parse_request(request);

    int failures = check_number("request_type", actual.request_type, test->expected.request_type);
    failures += check_string("URI", actual.URI, test->expected.URI);
    failures += check_fields(actual.fields, actual.fields_len, test->fields, test->expected.fields_len);

    free_HTTP_request_header(actual);
    free(request);
    return failures ? EXIT_FAILURE : EXIT_SUCCESS;
}

static int run_response_case(const response_case_t *test)
{
    char *response = malloc(strlen(test->response) + 1);
    if (response == NULL) return EXIT_FAILURE;
    strcpy(response, test->response);
    HTTP_response_header_t actual = HTTP_parse_response(response);

    int failures = check_number("status", actual.status, test->expected.status);
    failures += check_fields(actual.fields, actual.fields_len, test->fields, test->expected.fields_len);

    free_HTTP_response_header(actual);
    free(response);
    return failures ? EXIT_FAILURE : EXIT_SUCCESS;
}

int test_http_parse_request_case(const char *name)
{
    for (size_t i = 0; i < sizeof(request_cases) / sizeof(request_cases[0]); i++) {
        if (strcmp(name, request_cases[i].name) == 0) {
            fprintf(stderr, "Running %s\n", name);
            int result = run_request_case(&request_cases[i]);
            fprintf(stderr, "%s: %s\n", name, result == EXIT_SUCCESS ? "PASS" : "FAIL");
            return result;
        }
    }
    fprintf(stderr, "Unknown test case: %s\n", name);
    return EXIT_FAILURE;
}

int test_http_parse_request(void)
{
    size_t count = sizeof(request_cases) / sizeof(request_cases[0]);
    int failures = 0;
    for (size_t i = 0; i < count; i++) {
        failures += test_http_parse_request_case(request_cases[i].name) != EXIT_SUCCESS;
    }
    fprintf(stderr, "%zu request cases, %d failed\n", count, failures);
    return failures ? EXIT_FAILURE : EXIT_SUCCESS;
}

int test_http_parse_response_case(const char *name)
{
    for (size_t i = 0; i < sizeof(response_cases) / sizeof(response_cases[0]); i++) {
        if (strcmp(name, response_cases[i].name) == 0) {
            fprintf(stderr, "Running %s\n", name);
            int result = run_response_case(&response_cases[i]);
            fprintf(stderr, "%s: %s\n", name, result == EXIT_SUCCESS ? "PASS" : "FAIL");
            return result;
        }
    }
    fprintf(stderr, "Unknown response test case: %s\n", name);
    return EXIT_FAILURE;
}

int test_http_parse_response(void)
{
    size_t count = sizeof(response_cases) / sizeof(response_cases[0]);
    int failures = 0;
    for (size_t i = 0; i < count; i++) {
        failures += test_http_parse_response_case(response_cases[i].name) != EXIT_SUCCESS;
    }
    fprintf(stderr, "%zu response cases, %d failed\n", count, failures);
    return failures ? EXIT_FAILURE : EXIT_SUCCESS;
}

int test_http_create_case(const char *name)
{
    for (size_t i = 0; i < sizeof(create_cases) / sizeof(create_cases[0]); i++) {
        const create_case_t *test = &create_cases[i];
        if (strcmp(name, test->name) == 0) {
            fprintf(stderr, "Running %s\n", name);
            char *actual = test->type == HTTP_Request
                ? HTTP_create_request(test->header.request)
                : HTTP_create_response(test->header.response);
            int failures = check_string("serialized header", actual, test->expected);
            free(actual);
            fprintf(stderr, "%s: %s\n", name, failures ? "FAIL" : "PASS");
            return failures ? EXIT_FAILURE : EXIT_SUCCESS;
        }
    }
    fprintf(stderr, "Unknown creation test case: %s\n", name);
    return EXIT_FAILURE;
}

int test_http_create(void)
{
    size_t count = sizeof(create_cases) / sizeof(create_cases[0]);
    int failures = 0;
    for (size_t i = 0; i < count; i++) {
        failures += test_http_create_case(create_cases[i].name) != EXIT_SUCCESS;
    }
    fprintf(stderr, "%zu creation cases, %d failed\n", count, failures);
    return failures ? EXIT_FAILURE : EXIT_SUCCESS;
}
