# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build

```bash
# Configure with CMake (requires vcpkg at C:/vcpkg)
cmake -B out/build/x64-Debug -S . -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake \
  -DCMAKE_BUILD_TYPE=Debug

# Build
cmake --build out/build/x64-Debug

# Release (RelWithDebInfo)
cmake -B out/build/x64-Release -S . -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build out/build/x64-Release
```

**Dependencies:** Boost (system), nlohmann_json, pthreads — all via vcpkg. C++17 required.

There are no tests in this project.

## Architecture

This is a **WebSocket server** that acts as a backend for a QQ bot, speaking the **OneBot v11** protocol (Napcat framework). One thread per connection.

Currently it is a **minimal skeleton**: networking + send/receive only, with a single echo handler for verification. All business logic has been removed to start fresh.

```
WebSocket Client (Napcat/QQ)
    │
    ▼
BotClient (core/)          —— networking, WebSocket I/O, thread pool
    │
    ▼
CommandRouter (logic/)     —— event routing (currently: echo group messages back)
```

### File layout

- `main.cpp` — TCP listener; spawns a detached `BotClient` per accepted connection
- `core/bot_client.{h,cpp}` — per-connection message pipeline (3 threads: reader / workers / sender)
- `logic/command_router.{h,cpp}` — event routing / business entry point
- `logic/message_utils.hpp` — OneBot message construction helpers (namespace `msg`)
- `logic/logic_types.hpp` — type aliases (`json`, `ApiFunc`, `fs`)
- `network_types.hpp` — Boost.Beast / Asio aliases

### Data flow per connection

1. `main()` accepts TCP, upgrades to WebSocket, spawns a detached `BotClient`
2. `BotClient::start()` launches 3 internal thread types:
   - **reader** — reads JSON from WebSocket in a loop, enqueues to `event_queue_`
   - **workers** (`kPoolSize` threads) — dequeues events, calls `CommandRouter::handle()`
   - **sender** — dequeues `send_queue_`, applies rate-limiting delay, writes to WebSocket
3. `CommandRouter::handle()` routes by `event["post_type"]`; currently only echoes group messages

### API call pattern (echo-correlation)

Logic handlers receive an `ApiFunc` — they never touch the network directly. Calling `api("send_group_msg", params)` works like this:

1. `call_api()` generates a unique echo ID
2. Stores `std::promise<json>` in `pending_` map keyed by echo
3. Enqueues request JSON into `send_queue_` (goes through rate limiter)
4. Blocks on `future.get()`
5. `read_loop()` matches responses by echo ID and fulfills the promise

### Rate limiting

`sender_loop()` applies `kBaseDelay + rng() % kRandomDelay` seconds delay before sending. The delay values are hardcoded constants in `bot_client.cpp` (in an anonymous namespace).

## Configuration

Only `port.txt` is read at startup (in `main.cpp`), containing the TCP listen port. All other former config files have been removed.

## Key patterns

- **Static classes** — `CommandRouter` has no instances, only a static `handle()` method
- **Message construction** uses helper functions (`add_text_message`, `add_image_message`, `add_at_message`, `add_reply_message`) to build `json::array()` payloads
- Chinese-language comments and user-facing strings throughout
- Minimal error handling: `try/catch` → `cerr`/`cout` + `sleep_for` retry loops

## Namespace aliases (always available)

```cpp
// network_types.hpp
namespace beast = boost::beast;
namespace websocket = beast::websocket;
namespace net = boost::asio;
using tcp = net::ip::tcp;
using json = nlohmann::json;

// logic/logic_types.hpp
namespace fs = std::filesystem;
using ApiFunc = std::function<json(const std::string&, const json&)>;
```
