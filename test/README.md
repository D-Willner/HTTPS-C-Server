Tests for `HTTP_parse_request` cover its intended subset: GET and POST, URI
extraction, Host, User-Agent, Connection (`Keep-Alive` / `Close`), a numeric
cookie, text/html with UTF-8 and JSON content types, Content-Length, and extra
header fields. Fixtures use CRLF and the capitalization recognized by the code.
They do not require general cookie syntax, additional methods, chunked encoding,
or other unsupported HTTP features. Malformed-input behavior has no defined API
contract yet and is not tested.

From the repository root:

```sh
cmake -S . -B build
cmake --build build --target test_http
ctest --test-dir build --output-on-failure -R '^http_parse_request\.'
```

Each case runs separately with a five-second timeout so a parser crash or hang
does not prevent the remaining cases from running. Checks remain enabled in
release builds. The parser is currently unfinished; these are expected-behavior
tests. Its memory handling and loop termination need completing before these
tests can pass.

To run all cases from the application's `main.c`, include `test/test_http.h`
and return `test_http_parse_request()`. The server target currently does this
instead of starting the server. Build and run it with:

```sh
cmake --build build --target server
./build/server
```

`test_http.c` contains callable test functions; `test_main.c` supplies the separate
CTest executable's entry point. Both targets compile `http.c` once. Running all
cases in one process stops at the first parser crash; CTest runs cases separately.
