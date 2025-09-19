# js-server-on-c

A minimal HTTP server written in C and controlled from Bun (JavaScript) via FFI. The C server runs in a background thread; Bun handles startup and graceful shutdown.

- Language: C (server), JavaScript (Bun runtime)
- Port: 8080
- Platforms: macOS (.dylib) and Linux (.so)

## Prerequisites

- Bun (https://bun.sh) installed and available on PATH
- A C toolchain
  - macOS: Xcode Command Line Tools (clang)
  - Linux: gcc/clang and standard build tools

## Build the native library

Builds the platform-appropriate shared library (`tcp-ip.dylib` on macOS, `tcp-ip.so` on Linux):

```bash
bun run build
```

You can also build explicitly per OS:

```bash
bun run build:mac   # macOS
bun run build:linux # Linux
```

This compiles `tcp-ip.c` into a shared library that Bun can dlopen via FFI.

## Run the server

```bash
bun run start
```

Then in another terminal:

```bash
curl -i http://localhost:8080/
```

Expected response:

```
HTTP/1.1 200 OK
Content-Type: text/plain
Connection: close
Content-Length: 13

Hello, world!
```

Stop with Ctrl+C. Bun will call into the C `stop_server` function, which shuts down the listening socket and joins the server thread.

## How it works

- `server.js` uses Bun's `bun:ffi` to load the compiled C library and call two exported functions:
  - `start_server()` — starts the C server on a background pthread and returns immediately
  - `stop_server()` — sets a flag, shuts down the listening socket, and joins the thread
- `tcp-ip.c` implements a tiny HTTP server on port 8080 with these improvements:
  - SO_REUSEADDR/SO_REUSEPORT for fast restarts
  - EINTR-aware accept loop
  - Correct Content-Length calculation and Connection: close header
  - Graceful stop via shutdown() and thread join

On macOS, `server.js` loads `tcp-ip.dylib`; on Linux it loads `tcp-ip.so`. Windows is not currently supported.

## Project structure

- `server.js` — JS entrypoint (Bun). Starts and stops the C server.
- `tcp-ip.c` — C HTTP server implementation; exports `start_server` and `stop_server`.
- `package.json` — build and start scripts for Bun.
- `.gitignore` — ignores build outputs and binaries.

## Troubleshooting

- Port in use
  - macOS: `lsof -i :8080` then kill the PID
  - Linux: `ss -lptn 'sport = :8080'`
- Build errors on macOS
  - Ensure Command Line Tools are installed: `xcode-select --install`
- Using Node instead of Bun
  - This project requires Bun. `bun:ffi` is not available in Node.

## License

MIT

