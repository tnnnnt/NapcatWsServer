#pragma once
#include <cstdint>
#include <string>
#include "logic_types.hpp"

namespace msg {
	// 发送群消息
	inline json send_group_msg(const ApiFunc& api, int64_t group_id, const json& message) {
		return api("send_group_msg", {{"group_id", group_id}, {"message", message}});
	}
	// 发送私聊消息
	inline json send_private_msg(const ApiFunc& api, int64_t user_id, const json& message) {
		return api("send_private_msg", {{"user_id", user_id}, {"message", message}});
	}
	// 纯文本
	inline void add_text_message(json& message, const std::string& text) {
		message.emplace_back(json{{"type", "text"}, {"data", json{{"text", text}}}});
	}
	// 图片
	inline void add_image_message(json& message, const std::string& file_path) {
		message.emplace_back(json{{"type", "image"}, {"data", json{{"file", file_path}}}});
	}
	// @某人
	inline void add_at_message(json& message, int64_t qq) {
		message.emplace_back(json{{"type", "at"}, {"data", json{{"qq", qq}}}});
	}
	// 回复
	inline void add_reply_message(json& message, int message_id) {
		message.emplace_back(json{{"type", "reply"}, {"data", json{{"id", message_id}}}});
	}
} // namespace msg
