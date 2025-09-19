# Minimal HTTP/1.1 Server in C

This repository contains a small, educational HTTP/1.1 server written in C. It's designed for learning how HTTP works end-to-end: accepting TCP connections, parsing requests, routing, serving files, and writing correct responses.

Highlights:
- Multi-threaded request handling using pthreads (one thread per connection)
- Basic routing with GET/HEAD for common endpoints
- File serving (GET/HEAD /files/<name>) from a configurable directory
- Simple file upload (POST /files/<name>) using Content-Length
- Correct HTTP/1.1 framing (CRLFs, Content-Length, Connection: close)

## Getting started

Requirements:
- CMake 3.13+
- A C compiler (clang or gcc)

Build & run locally:

```sh
./your_program.sh --directory ./tmp
```

Under the hood, this script runs CMake and executes the built binary at `build/http-server`.

Alternatively, manual steps:

```sh
cmake -B build -S .
cmake --build ./build
./build/http-server --directory ./tmp
```

By default the server listens on port 4221.

## Endpoints

- GET / -> 200 OK, empty body
- GET /echo/<text> -> echoes <text> (Content-Type: text/plain)
- HEAD /echo/<text> -> same headers as GET, no body
- GET /user-agent -> returns the User-Agent request header
- GET /files/<filename> -> returns file bytes from the configured directory
- HEAD /files/<filename> -> same headers as GET, no body
- POST /files/<filename> -> writes request body to the configured directory (uses Content-Length), responds 201 Created

Notes:
- File paths are validated to prevent path traversal. Only simple names under the configured directory are allowed (no "..", no absolute paths).
- All responses include `Connection: close`.

## Usage examples

Assuming the server is running with `--directory ./tmp`:

```sh
# Root
curl -i http://localhost:4221/

# Echo
curl -i http://localhost:4221/echo/hello

# User-Agent
curl -i -H "User-Agent: my-client" http://localhost:4221/user-agent

# Serve a file
printf "hello file" > tmp/hello.txt
curl -i http://localhost:4221/files/hello.txt

# HEAD for a file (headers only)
curl -I http://localhost:4221/files/hello.txt

# Upload a file via POST
curl -i -X POST --data-binary @tmp/hello.txt http://localhost:4221/files/uploaded.txt
```

## Design overview

- The server listens on TCP port 4221 and enables SO_REUSEADDR for convenience.
- Each accepted client socket is handled on a detached pthread.
- Requests are parsed from the socket until headers are complete ("\r\n\r\n").
- Routing is simple string matching on the request path.
- Responses are written with correct Content-Length and terminated with CRLFCRLF.

## Limitations and ideas for improvement

- No chunked transfer encoding or persistent connections (keep-alive) yet.
- No gzip/deflate support (Accept-Encoding) yet.
- Request parsing is minimal and not fully robust to all edge cases.
- Error responses are intentionally simple.

Possible next steps for learning:
- Add support for gzip when Accept-Encoding includes "gzip" (via zlib)
- Implement keep-alive and a simple connection pool
- Parse headers case-insensitively and handle repeated headers
- Add a tiny router and a proper request/response abstraction
- Add unit tests and benchmarks

## Project layout

- `src/main.c` - All server logic (socket setup, request handling, routing)
- `CMakeLists.txt` - Build configuration
- `your_program.sh` - Helper script to build & run locally
