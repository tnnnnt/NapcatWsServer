# DEVELOPMENT.md — 业务开发指南

本文档面向「在这个 QQ 机器人项目里写业务功能」，告诉你在哪写、怎么写、有哪些现成工具可用，并附一份 OneBot v11 常用字段速查表。

## 一、你只需要关注这些文件

| 文件 | 你的角色 | 什么时候动 |
|---|---|---|
| `logic/command_router.cpp` | **写业务逻辑的主战场** | 加新功能、改回显逻辑都在这里 |
| `logic/command_router.h` | 声明入口 | 一般不用动（`handle()` 签名固定） |
| `logic/message_utils.hpp` | 现成的收发/构造助手（`msg::`） | 只有需要「新的发送/构造能力」时才加函数 |
| `core/bot_client.*`、`main.cpp` | 网络骨架 | **基本不用碰**，除非要改线程/连接模型 |

**一句话：业务写在 `command_router.cpp`，收发消息用 `msg::` 助手，其余不用管。**

---

## 二、业务逻辑写在哪、怎么写

所有业务从 `CommandRouter::handle()` 进入。它的签名是：

```cpp
void CommandRouter::handle(const json& event, const ApiFunc& api);
```

- `event`：Napcat 推来的一条完整事件 JSON（消息、通知、请求等）。
- `api`：调用 QQ API 的统一入口，签名 `json api(const std::string& action, const json& params)`。**不要直接碰网络**，发消息、查信息都通过它。

### 2.1 推荐的 handle() 结构

```cpp
void CommandRouter::handle(const json& event, const ApiFunc& api) {
    // 第一步：按 post_type 分流（消息 / 通知 / 请求 / 心跳）
    const std::string post_type = event.at("post_type").get<std::string>();
    if (post_type == "message") {
        handle_message(event, api);
    }
    else if (post_type == "notice") {
        handle_notice(event, api);
    }
    // 其余类型暂不处理
}

// 第二步：消息里再按 message_type 分流（私聊 / 群聊）
void handle_message(const json& event, const ApiFunc& api) {
    const std::string message_type = event.at("message_type").get<std::string>();
    if (message_type == "private") {
        // 私聊处理
    }
    else if (message_type == "group") {
        // 群聊处理
    }
}
```

> 提示：如果 `handle()` 里逻辑变长，建议把「私聊处理」「群聊处理」拆成独立的辅助函数（放匿名命名空间），保持 `handle()` 本身只是「分流」。

### 2.2 一个完整的群聊回显示例

从「私聊回显」改成「群聊回显」，只需把 `message_type` 判断、字段、发送函数换掉：

```cpp
void handle_group_echo(const json& event, const ApiFunc& api) {
    const int64_t group_id = event.at("group_id").get<int64_t>();  // 群号
    const std::string raw_message = event.at("raw_message").get<std::string>(); // 纯文本内容

    json message = json::array();                       // 消息数组（可塞多段）
    msg::add_text_message(message, "收到：" + raw_message); // 加一段文本
    msg::send_group_msg(api, group_id, message);        // 发到群里
}
```

---

## 三、现成的工具：`msg::` 助手

都在 `logic/message_utils.hpp`，命名空间 `msg`，`inline` 函数，直接调用。

### 发送类

| 函数 | 作用 |
|---|---|
| `json send_group_msg(api, group_id, message)` | 发群消息 |
| `json send_private_msg(api, user_id, message)` | 发私聊消息 |

两者返回值都是 API 回包 `json`，可用 `resp["data"]["message_id"]` 拿到消息 id（用于回复引用等）。

### 构造类（往 `message` 数组里追加消息段）

```cpp
json message = json::array(); // 先建一个空消息数组

msg::add_text_message(message, "你好");              // 文本
msg::add_image_message(message, "file:///path/x.png"); // 图片（本地路径）
msg::add_at_message(message, 123456789);            // @某人（QQ号）
msg::add_reply_message(message, message_id);         // 回复某条消息（引用）
```

> 消息数组里的元素叫「消息段（segment）」，一条消息可以由多个段拼接，例如「@某人 + 一段文字 + 一张图」。多个 `add_*` 按顺序追加即可。

### 扩展新助手

如果要发「语音」「表情」等，仿照现有写法在 `message_utils.hpp` 里加一个 `add_*` / `send_*` 即可。例如表情：

```cpp
// 表情（face id 见 QQ 表情编号）
inline void add_face_message(json& message, int face_id) {
    message.emplace_back(json{{"type", "face"}, {"data", json{{"id", face_id}}}});
}
```

---

## 四、字段速查表

> 只列实际开发常用的字段。完整协议见 OneBot v11 文档（`post_type` 的取值是协议约定，Napcat 兼容）。

### 4.1 事件顶层字段（所有事件都有）

| 字段 | 类型 | 说明 |
|---|---|---|
| `post_type` | string | 事件大类：`message` / `notice` / `request` / `meta_event` |
| `time` | int64 | 事件发生时间（Unix 秒） |
| `self_id` | int64 | 机器人自己的 QQ 号 |

### 4.2 消息事件（`post_type == "message"`）常用字段

| 字段 | 类型 | 说明 |
|---|---|---|
| `message_type` | string | `private`（私聊）/ `group`（群聊） |
| `sub_type` | string | 子类型（如群聊的 `normal`、`anonymous` 等） |
| `user_id` | int64 | 发送者 QQ 号 |
| `group_id` | int64 | 群号（**仅群聊有**） |
| `raw_message` | string | 纯文本内容（不含图片、@ 等富文本） |
| `message` | array | 消息段数组（见 4.3） |
| `message_id` | int64 | 这条消息的 id（可用于回复引用） |

### 4.3 消息段 `message`（数组元素）常用 type

每个元素形如 `{"type": "...", "data": {...}}`。

| type | data 关键字段 | 说明 |
|---|---|---|
| `text` | `text` (string) | 文本 |
| `image` | `file`、`url` | 图片 |
| `at` | `qq` (string) | @某人，`qq` 是目标 QQ 号（字符串） |
| `reply` | `id` | 回复/引用，`id` 是被回复消息的 id |
| `face` | `id` (string) | QQ 表情，`id` 是表情编号 |

> 注意类型：`at` 的 `qq`、`face` 的 `id` 在事件里通常是 **string**，用 `msg.at("data").at("qq").get<std::string>()` 读取；而 `event` 顶层的 `group_id`/`user_id` 是 **int64**。

### 4.4 通知事件（`post_type == "notice"`）常用字段

| 字段 | 说明 |
|---|---|
| `notice_type` | 通知类型：`group_increase`（进群）/ `group_decrease`（退群）/ `group_ban`（禁言）/ 等 |
| `group_id` | 群号 |
| `user_id` | 相关用户 QQ 号 |

### 4.5 常用 API（通过 `api(action, params)` 调用）

| action | params 关键字段 | 说明 |
|---|---|---|
| `send_group_msg` | `group_id`, `message` | 发群消息 |
| `send_private_msg` | `user_id`, `message` | 发私聊消息 |
| `get_group_member_info` | `group_id`, `user_id` | 查群成员信息（昵称、名片等） |
| `get_group_member_list` | `group_id` | 查群成员列表 |
| `get_stranger_info` | `user_id` | 查陌生人信息（昵称等） |

> 这些 API 的回包里，业务数据在 `resp["data"]`，错误码在 `resp["retcode"]`（0 表示成功）。

---

## 五、开发流程小结

1. **加一个功能** = 在 `command_router.cpp` 里新增一个辅助函数（匿名命名空间），并在 `handle()` / `handle_message()` 里加上对应的分流分支。
2. **发消息** = 用 `msg::send_*`；**拼内容** = 用 `msg::add_*` 往 `json::array()` 里塞段。
3. **要新的收发/构造能力** = 在 `message_utils.hpp` 的 `msg::` 里加一个 `inline` 函数。
4. **查字段** = 看上面第四节速查表；不确定就临时在 `handle()` 里 `std::cout << event.dump(4)` 打印完整事件 JSON 看结构。

> 调试技巧：想知道某个事件到底长什么样，可以在 `handle()` 开头加一行 `std::cout << event.dump(4) << std::endl;`，看打印出的完整 JSON，比查文档更直观。
