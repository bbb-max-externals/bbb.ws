## Project

Max/MSP external for WebSocket communication. Uses min-api + CMake + IXWebSocket.
Cross-platform: macOS (x86_64 + arm64) + Windows (x64). WSS (TLS) supported.

## Externals

| External | 役割 |
|---|---|
| `bbb.ws.client` | WebSocket クライアント（送受信統合） |
| `bbb.ws.server` | WebSocket サーバー（ブラウザ等からの接続を受け付ける） |

## Skills

このリポジトリの技能はすべて submodule (`deps/` ではなく `.agents/skills/` 以下) にある。

| 技能 | 用途 |
|---|---|
| `max-external` | external の新規作成・ビルド手順。テンプレート・CMake構造・命名規則の全容 |
| `max-patgen` | `.maxpat` / `.maxhelp` の JSON 生成 |
| `max-external-githubactions` | CI 用 workflow テンプレート |

external を追加・修正するときは **必ず `max-external` 技能を読み込むこと**。
また `max-external/docs/pitfalls.md` のトラップ集を必ず参照。

## Build

```bash
mkdir -p build && cd build
cmake ..
cmake --build .
```

成果物: `externals/*.mxo` (universal binary: x86_64 + arm64)

## Dependencies

| 依存 | 配置 | ライセンス |
|---|---|---|
| min-api | `deps/min-api/` (submodule) | MIT |
| IXWebSocket | `deps/IXWebSocket/` (submodule) | BSD 3-Clause |

### TLS (WSS) 設定

IXWebSocket はプラットフォーム別に TLS バックエンドを切り替える:

- **macOS**: Secure Transport (OS ネイティブ、`-framework Security -framework Foundation`)
- **Windows**: mbedTLS (Apache 2.0、~420 KB)

CMake 設定:
- macOS: `-DUSE_TLS=1`（Secure Transport が自動選択される）
- Windows: `-DUSE_TLS=1 -DUSE_MBED_TLS=1`

## Critical Constraints

- **`std::filesystem` は使えない**。min-api が `CMAKE_OSX_DEPLOYMENT_TARGET` を `10.11` に強制するため。パス操作は `c74::min::path` または `std::string` で代替。
- **outlet 出力はメインスレッドのみ**。IXWebSocket のコールバックはバックグラウンドスレッドで発火する。`c74::min::queue<>` でメインスレッドに受け渡すこと。
- **送信はメインスレッドから直接可**。`ix::WebSocket::send()` はスレッドセーフ。
- **attribute はコンストラクタ完了後に設定される**。初期化は `timer.delay(0)` で遅延させる。
- **`cout` / `cerr` はメンバ変数**。`std::cout` は Max コンソールに出ない。
- **`enum_map` を使う**。`range{"a","b"}` + `style::enum_index` は "bad number" エラー。
- **`NIL` マクロ衝突**。IXWebSocket インクルード前に `#pragma push_macro("NIL")` / `#undef NIL` すること。

## Project Structure

```
bbb.ws/
├── CMakeLists.txt                  # root (auto-discovers source/projects/*)
├── cmake/
│   └── bbb_external.cmake          # bbb_add_external() definition
├── deps/
│   ├── min-api/                    # git submodule
│   └── IXWebSocket/               # git submodule
├── source/
│   ├── projects/
│   │   ├── bbb.ws.client/          # WS client (send + receive)
│   │   │   ├── CMakeLists.txt
│   │   │   └── bbb.ws.client.cpp
│   │   └── bbb.ws.server/          # WS server
│   │       ├── CMakeLists.txt
│   │       └── bbb.ws.server.cpp
│   └── bbb/                        # shared headers (optional)
├── externals/                      # build output (*.mxo / *.mxe64)
├── help/                           # .maxhelp copies
├── package-info.json
└── .agents/skills/                 # skills (git submodules)
```

新しい external を追加するときは `source/projects/<name>/` ディレクトリを作るだけで、
root CMakeLists.txt の `SUBDIRLIST` が自動認識する。

## bbb.ws.client — Client API

### Inlet (left)

| メッセージ | 説明 |
|---|---|
| `connect` | `@url` に接続 |
| `disconnect` | 切断 |
| `send <anything>` | テキストフレーム送信 |
| `send_bytes <int ...>` | バイナリフレーム送信 |
| (anything) | テキストフレームとして送信 |

### Outlets

- **Left**: 受信メッセージ (text → symbol, binary → int list)
- **Right**: ステータス (`connected <url>`, `disconnected <code> <reason>`, `error <message>`)

### Attributes

| 属性 | 型 | デフォルト | 説明 |
|---|---|---|---|
| `@url` | symbol | `""` | 接続先 (`ws://` or `wss://`) |
| `@auto_connect` | bool | `true` | ロード時自動接続 |
| `@reconnect_interval` | int | `5000` | 再接続間隔 ms。`0` で無効 |
| `@binary` | bool | `false` | anything メッセージの送信モード |
| `@subprotocol` | symbol | `""` | `Sec-WebSocket-Protocol` |

### Thread Model

```
IXWebSocket BG thread              Max main thread
       │                                │
  onMessage() ────┐                   │
  onOpen()        │  queue<>.set()    │
  onClose()       └──────────────────►├─ queue callback → outlet output
  onError()                          │
       │   ws.send() ◄── direct ─────┤─ message handler (thread-safe)
```

## bbb.ws.server — Server API

### Inlet (left)

| メッセージ | 説明 |
|---|---|
| `broadcast <anything>` | 全クライアントに送信 |
| `send <client_id> <anything>` | 特定クライアントに送信 |
| `close <client_id>` | クライアント切断 |
| (anything) | broadcast のエイリアス |

### Outlets

- **Left**: 受信メッセージ (`<client_id> <message>`)
- **Right**: 接続イベント (`connected <client_id>`, `disconnected <client_id>`)

### Attributes

| 属性 | 型 | デフォルト | 説明 |
|---|---|---|---|
| `@port` | int | `8080` | リスンポート |
| `@bind_ip` | symbol | `"0.0.0.0"` | バインドIF |
| `@max_clients` | int | `10` | 最大同時接続数 |
| `@tls` | bool | `false` | WSS 有効化 |
| `@cert_file` | symbol | `""` | TLS 証明書パス |
| `@key_file` | symbol | `""` | TLS 秘密鍵パス |
| `@subprotocol` | symbol | `""` | `Sec-WebSocket-Protocol` |
