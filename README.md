# bbb.ws

> [!WARNING]
> This repository is published as AI-assisted, insufficiently tested work in progress ("AI slop"). Treat it as experimental. Correctness, stability, compatibility, and fitness for production use are not guaranteed.

WebSocket client and server externals for [Max/MSP](https://cycling74.com/products/max).

## Overview

**bbb.ws.client** connects to a WebSocket server and sends/receives text and binary frames.
**bbb.ws.server** listens for incoming connections and broadcasts or sends to individual clients.

Both support TLS (WSS), auto-reconnect, and subprotocol negotiation.

## Build

### Prerequisites

- CMake ≥ 3.19
- macOS: Xcode Command Line Tools
- Windows: Visual Studio 2022 with C++ workload

### Build

```bash
git clone --recursive https://github.com/bbb-max-externals/bbb.ws.git
cd bbb.ws
cmake -B build
cmake --build build --config Release
```

Output: `externals/bbb.ws.client.mxo`, `externals/bbb.ws.server.mxo` (macOS universal: x86_64 + arm64), or `.mxe64` (Windows x64).

### CMake options

| Option | Default | Description |
|--------|---------|-------------|
| `USE_TLS` | `ON` | Enable WSS support |
| `USE_MBED_TLS` | auto | Use mbedTLS (Windows default) |

### Windows build (mbedTLS)

On Windows, mbedTLS is required for TLS. Install via [vcpkg](https://vcpkg.io):

```bash
vcpkg install mbedtls:x64-windows-static
cmake -B build -G "Visual Studio 17 2022" -A x64 ^
  -DUSE_TLS=1 -DUSE_MBED_TLS=1 ^
  -DCMAKE_TOOLCHAIN_FILE=<vcpkg-root>/scripts/buildsystems/vcpkg.cmake
cmake --build build --config Release
```

## Installation

Copy the contents of `externals/` and `help/` into your Max Packages folder, or use the package as a git submodule.

## bbb.ws.client

WebSocket client. Connects to a server, sends messages, and receives responses.

### Messages

| Inlet message | Description |
|---|---|
| `connect` | Connect to `@url` |
| `disconnect` | Disconnect |
| `send <anything>` | Send text frame (all args concatenated) |
| `send_bytes <int ...>` | Send binary frame |
| *(anything)* | Send as text frame (or binary if `@binary 1`) |

### Outlets

- **Left**: Text arrives as `text <UTF-8 byte ...>`; binary remains an integer list. For example, "hi" arrives as `text 104 105`. Route the fixed `text` selector instead of treating the payload as a symbol.
- **Right**: `connected <url>`, `disconnected <code> <reason bytes ...>`, `error <HTTP status> <reason bytes ...>`, or `queue_overflow`. Diagnostic text is limited to 256 bytes.

### Attributes

| Attribute | Type | Default | Description |
|---|---|---|---|
| `@url` | symbol | `""` | WebSocket URL (`ws://` or `wss://`) |
| `@auto_connect` | bool | `true` | Connect on load |
| `@reconnect_interval` | int | `5000` | Reconnect interval in ms (`0` = disabled) |
| `@max_message_size` | int | `16384` | Incoming bytes per message; 1–16384, applied on connect |
| `@binary` | bool | `false` | Send mode for typed messages |
| `@subprotocol` | symbol | `""` | `Sec-WebSocket-Protocol` header |

## bbb.ws.server

WebSocket server. Accepts connections from browsers or other clients.

### Messages

| Inlet message | Description |
|---|---|
| `start` | Start listening on `@port` |
| `stop` | Stop the server |
| `broadcast <anything>` | Send to all connected clients |
| `send <client_id> <anything>` | Send to a specific client |
| `close <client_id>` | Disconnect a client |
| *(anything)* | Alias for `broadcast` |

### Outlets

- **Left**: `<client_id> text <UTF-8 byte ...>` for text, or `<client_id> <byte ...>` for binary. Received text is no longer interned as a Max symbol.
- **Right**: Connection events (`connected <client_id>`, `disconnected <client_id>`)

### Attributes

| Attribute | Type | Default | Description |
|---|---|---|---|
| `@port` | int | `8080` | Listen port |
| `@bind_ip` | symbol | `"0.0.0.0"` | Listens on **all interfaces**. Use `@bind_ip 127.0.0.1` for local patches. |
| `@max_message_size` | int | `16384` | Incoming bytes per message per client; 1–16384, applied on start |
| `@max_clients` | int | `10` | Max concurrent connections |
| `@tls` | bool | `false` | Enable WSS |
| `@cert_file` | symbol | `""` | TLS certificate path |
| `@key_file` | symbol | `""` | TLS private key path |
| `@subprotocol` | symbol | `""` | `Sec-WebSocket-Protocol` |

Both externals close oversized messages with code 1009. The 16 KiB maximum keeps one complete message and its routing atoms within the Max outlet list length; split larger application payloads across separate messages. Each external buffers at most 64 undelivered events. A full queue drops the oldest event and reports `queue_overflow` on the status outlet at most once per second. The server shares this queue across its clients. Received strings are sent as bytes because Max retains interned symbols for the process lifetime.

## Project Structure

```
bbb.ws/
├── CMakeLists.txt
├── cmake/bbb_external.cmake
├── source/projects/
│   ├── bbb.ws.client/
│   └── bbb.ws.server/
├── deps/
│   ├── min-api/          (submodule)
│   └── IXWebSocket/      (submodule)
├── externals/            (build output)
├── help/
└── tests/                (Node.js test scripts)
```

## License

MIT © 2bit, 2026

### Third-party licenses

| Library | License | Notes |
|---|---|---|
| [min-api](https://github.com/Cycling74/min-api) | MIT | by Cycling '74 |
| [IXWebSocket](https://github.com/machinezone/IXWebSocket) | BSD 3-Clause | by Machine Zone |
| [mbedTLS](https://github.com/Mbed-TLS/mbedtls) | Apache 2.0 | Windows TLS backend only |
