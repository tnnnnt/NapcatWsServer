#include "command_router.h"
#include "message_utils.hpp"
constexpr int64_t TEST_GROUP = 1082705308;// 测试群号，仅该群触发回显，避免打扰其他群

static void handle_message(const json& event, const ApiFunc& api) {
	const std::string message_type = event.at("message_type").get<std::string>();
	if (message_type == "private") {
		// 私聊处理
	}
	else if (message_type == "group") {
		const int64_t group_id = event.at("group_id").get<int64_t>();
		// 测试回显
		if (group_id == TEST_GROUP) {
			json message = json::array();
			msg::add_text_message(message, event.dump(4));
			msg::send_group_msg(api, group_id, message);
		}
	}
}

static void handle_notice(const json& event, const ApiFunc& api) {
	// todo
}

void CommandRouter::handle(const json& event, const ApiFunc& api) {
	const std::string post_type = event.at("post_type").get<std::string>();
	if (post_type == "message") {
		handle_message(event, api);
	}
	else if (post_type == "notice") {
		handle_notice(event, api);
	}
}
