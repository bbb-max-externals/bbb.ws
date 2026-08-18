# bbb.ws

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

- **Left**: Received messages (text → symbol, binary → int list)
- **Right**: Status (`connected <url>`, `disconnected <code> <reason>`, `error <message>`)

### Attributes

| Attribute | Type | Default | Description |
|---|---|---|---|
| `@url` | symbol | `""` | WebSocket URL (`ws://` or `wss://`) |
| `@auto_connect` | bool | `true` | Connect on load |
| `@reconnect_interval` | int | `5000` | Reconnect interval in ms (`0` = disabled) |
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

- **Left**: Received messages (`<client_id> <message>`)
- **Right**: Connection events (`connected <client_id>`, `disconnected <client_id>`)

### Attributes

| Attribute | Type | Default | Description |
|---|---|---|---|
| `@port` | int | `8080` | Listen port |
| `@bind_ip` | symbol | `"0.0.0.0"` | Bind interface |
| `@max_clients` | int | `10` | Max concurrent connections |
| `@tls` | bool | `false` | Enable WSS |
| `@cert_file` | symbol | `""` | TLS certificate path |
| `@key_file` | symbol | `""` | TLS private key path |
| `@subprotocol` | symbol | `""` | `Sec-WebSocket-Protocol` |

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
