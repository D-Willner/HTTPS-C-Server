The request tests check GET and POST, URI extraction, and the ordered header
name/value pairs in `fields`. The response tests check status codes, both
status-line layouts accepted by the parser, and the ordered header pairs,
including repeated `Set-Cookie` fields. Both suites use CRLF fixtures and check
that parsing stops at the header/body boundary.

The creation tests compare the complete HTTP/1.1 request and response header
strings, including the start line, field order, and final blank line. They cover
headers with no fields, multiple fields, empty values, and repeated names.

From the repository root:

```sh
cmake -S . -B build
cmake --build build --target test_http
ctest --test-dir build --output-on-failure
```

CTest runs each case in a separate process with a five-second timeout. Checks
remain enabled in release builds. `./build/test_http` runs all suites in one
process. Run the creation cases alone with `./build/test_http create` or
`ctest --test-dir build --output-on-failure -R '^http_create_'`.

To run all cases from the application's `main.c`, include `test/test_http.h`
and return `test_http_parse_request()`. The server target currently does this
instead of starting the server. Build and run it with:

```sh
cmake --build build --target server
./build/server
```

`test_http.c` contains callable test functions. `test_main.c` supplies the
separate CTest executable's entry point.
