#include "../command_router.h"
#include "../common.hpp"
#include "handle_notice.h"
#include <cstdlib>

void HandleNotice::start(const json& event, std::function<json(const std::string&, const json&)> api) {
	const std::string notice_type = event.at("notice_type").get<std::string>();
	if (notice_type == "group_upload") {}
	else if (notice_type == "group_admin") {}
	else if (notice_type == "group_decrease") {
		const auto group_id = event.at("group_id").get<int64_t>();
		const auto user_id = event.at("user_id").get<int64_t>();
		CommandRouter::del_today_group_member_message_number_data(group_id, user_id);
		const std::string nick = api("get_stranger_info", json{ {"user_id", user_id} })["data"].at("nick").get<std::string>();
		json params{};
		params["group_id"] = group_id;
		json message = json::array();
		common::add_text_message(message, "【" + nick + "】(" + std::to_string(user_id) + ") 遗憾离场 v_v");
		params["message"] = message;
		const int message_id = api("send_group_msg", params)["data"].at("message_id").get<int>();
		CommandRouter::del_notice_group_member(group_id, user_id);
		std::vector<int64_t> user_ids;
		CommandRouter::get_notice_members_by_group(group_id, user_ids);
		const int user_count = user_ids.size();
		for (int i = 0; i < user_count; ++i) {
			if (i % 20 == 0) {
				message.clear();
				common::add_reply_message(message, message_id);
			}
			common::add_at_message(message, user_ids[i]);
			if ((i + 1) % 20 == 0 || i == user_count - 1) {
				params["message"] = message;
				api("send_group_msg", params);
			}
		}
	}
	else if (notice_type == "group_increase") {
		const auto group_id = event.at("group_id").get<int64_t>();
		const auto user_id = event.at("user_id").get<int64_t>();
		json params{};
		params["group_id"] = group_id;
		json message = json::array();
		common::add_at_message(message, user_id);
		common::add_text_message(message, " 欢迎喵~爱你喵~");
		params["message"] = message;
		api("send_group_msg", params);
	}
	else if (notice_type == "group_ban") {}
	else if (notice_type == "friend_add") {}
	else if (notice_type == "group_recall") {}
	else if (notice_type == "friend_recall") {}
	else if (notice_type == "notify") {}
}