# c-fun

Small experiments with C and JavaScript interop.

Inner projects:
- js-server-on-c — Minimal HTTP server written in C and controlled from Bun via FFI (port 8080). See js-server-on-c/README.md.
- c-server (folder: server) — Standalone HTTP/1.1 server in C using pthreads (port 4221). Build with CMake and run via ./your_program.sh. See server/README.md.
- text-editor — Terminal-based text editor written in C with vim-like navigation, file operations, and status bar. Features include cursor movement, text editing, file save/load, and visual feedback.
