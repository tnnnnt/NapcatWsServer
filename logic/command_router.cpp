#include "command_router.h"
#include "message_utils.hpp"

void CommandRouter::handle(const json& event, const ApiFunc& api) {
	const std::string post_type = event.at("post_type").get<std::string>();
	if (post_type != "message") {
		// 只处理消息事件；notice（通知）、request（好友请求）、meta_event（心跳）等暂不处理
		return;
	}

	const std::string message_type = event.at("message_type").get<std::string>();
	if (message_type != "private") {
		// 只回显私聊消息（用于验证链路）；群聊暂不处理，避免打扰群里的人
		return;
	}

	const int64_t user_id = event.at("user_id").get<int64_t>();
	const std::string raw_message = event.at("raw_message").get<std::string>();

	// 最小可验证的 echo：把收到的原文私聊发回给对方
	json message = json::array();
	msg::add_text_message(message, "收到：" + raw_message);
	msg::send_private_msg(api, user_id, message);
}
