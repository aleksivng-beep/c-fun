const { dlopen, FFIType } = require('bun:ffi');
const path = require('path');

const symbols = {
  start_server: {
    args: [],
    returns: FFIType.void,
  },
  stop_server: {
    args: [],
    returns: FFIType.void,
  },
};

function resolveLibPath() {
  const ext = process.platform === 'darwin'
    ? '.dylib'
    : process.platform === 'win32'
    ? '.dll'
    : '.so';
  return path.join(__dirname, `tcp-ip${ext}`);
}

const libPath = resolveLibPath();
const lib = dlopen(libPath, symbols);

// Start the C server in background (C will spawn a thread and return immediately)
lib.symbols.start_server();

// Graceful shutdown on common termination signals
function shutdown() {
  try {
    lib.symbols.stop_server();
  } catch {}
}

process.on('SIGINT', shutdown);
process.on('SIGTERM', shutdown);

// Keep the process alive; the C server is running in its own thread
// and will exit when stop_server() is called.
setInterval(() => {}, 1 << 30);
