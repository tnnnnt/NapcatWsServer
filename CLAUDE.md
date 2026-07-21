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

```
WebSocket Client (Napcat/QQ)
    │
    ▼
BotClient (core/)          —— networking, WebSocket I/O, thread pool
    │
    ▼
CommandRouter (logic/)     —— event routing by post_type
    ├── HandleMessage      —— ~15 group-chat commands
    └── HandleNotice       —— group event notices
```

### Data flow per connection

1. `main()` accepts TCP, upgrades to WebSocket, spawns a detached `BotClient`
2. `BotClient::start()` launches 5 internal thread types:
   - **reader** — reads JSON from WebSocket in a loop, enqueues to `event_queue_`
   - **workers** (POOL_SIZE threads) — dequeues events, calls `CommandRouter::handle()` → `HandleMessage`/`HandleNotice`
   - **sender** — dequeues `send_queue_`, applies random delay for rate limiting, writes to WebSocket
   - **midnight task** — sleeps to next midnight, triggers daily reset
   - **periodic task** — reloads config periodically, persists message counts
3. `CommandRouter::handle()` routes by `event["post_type"]`: `"message"` → `HandleMessage`, `"notice"` → `HandleNotice`

### API call pattern (echo-correlation)

Logic handlers receive an `ApiFunc` — they never touch the network directly. Calling `api("send_group_msg", params)` works like this:

1. `call_api()` generates a unique echo ID
2. Stores `std::promise<json>` in `pending_` map keyed by echo
3. Enqueues request JSON into `send_queue_` (goes through rate limiter)
4. Blocks on `future.get()`
5. `read_loop()` matches responses by echo ID and fulfills the promise

### Rate limiting

`sender_loop()` applies `BASE_DELAY + rand() % RANDOM_DELAY` seconds delay before `send_private_msg`, `send_group_msg`, or `send_msg` actions.

## Configuration system (runtime)

All config lives under `/home/bot/qq_robot/` on the production server:

| Config | When loaded | Contents |
|---|---|---|
| `port.txt` | Startup | TCP listen port |
| `config_static.json` | Startup only | admin QQ, robot QQ, pool size, rank size, test group |
| `config_periodic.json` | Periodically | base_delay, random_delay, time_save_interval |
| `config_daily.json` | Midnight | min_activity_level |
| `save/ban.txt` | Startup + on change | Banned user IDs |
| `save/fortunes.txt` | Once per day | Fortune phrases |
| `save/today_group_member_message_number.json` | Periodically | Persisted daily message counts |

## Key patterns

- **Global state in `logic/common.hpp`** — all shared data uses `inline` globals with mutexes for thread safety (`group_members_mutex`, `today_group_member_message_number_mutex`, `group_member_relations_mutex`)
- **Static classes only** — `CommandRouter`, `HandleMessage`, `HandleNotice` have no instances, only static methods
- **Command matching** is literal string comparison on `raw_message` (e.g., `"今日运势"`, `"来点色图"`)
- **Message construction** uses helper functions (`add_text_message`, `add_image_message`, `add_at_message`, `add_reply_message`) to build `json::array()` payloads
- **Python interop** — `script/relations.py` renders relationship graphs via pyvis + Playwright, invoked by C++ via `system()`
- Chinese-language comments and user-facing strings throughout
- Minimal error handling: `try/catch` → `cerr` + `sleep_for` retry loops

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
