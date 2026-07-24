#include "handle_notice.h"
#include <cstdlib>
#include "../command_router.h"
#include "../common.hpp"

void HandleNotice::start(const json& event, const ApiFunc& api) {
	const std::string notice_type = event.at("notice_type").get<std::string>();
	if (notice_type == "group_upload") {
	}
	else if (notice_type == "group_admin") {
	}
	else if (notice_type == "group_decrease") {
		const auto group_id = event.at("group_id").get<int64_t>();
		const auto user_id = event.at("user_id").get<int64_t>();
		const std::string nick =
			api("get_stranger_info", json{{"user_id", user_id}})["data"].at("nick").get<std::string>();
		json message = json::array();
		common::add_text_message(message, "【" + nick + "】(" + std::to_string(user_id) + ") 遗憾离场 v_v");
		const int message_id = common::send_group_msg(api, group_id, message)["data"].at("message_id").get<int>();
		const std::string group_id_str = std::to_string(group_id);
		const std::string user_id_str = std::to_string(user_id);
		std::vector<int64_t> user_ids;
		if (common::notice_group_member_json.contains(group_id_str)) {
			auto& members = common::notice_group_member_json[group_id_str];
			members.erase(std::remove(members.begin(), members.end(), user_id_str), members.end());
			CommandRouter::save_notice_group_member_data();
			for (const auto& user_id_str : members) {
				user_ids.push_back(std::stoll(user_id_str.get<std::string>()));
			}
		}
		const int user_count = user_ids.size();
		for (int i = 0; i < user_count; ++i) {
			if (i % 20 == 0) {
				message.clear();
				common::add_reply_message(message, message_id);
			}
			common::add_at_message(message, user_ids[i]);
			if ((i + 1) % 20 == 0 || i == user_count - 1) {
				common::send_group_msg(api, group_id, message);
			}
		}
	}
	else if (notice_type == "group_increase") {
		const auto group_id = event.at("group_id").get<int64_t>();
		const auto user_id = event.at("user_id").get<int64_t>();
		json message = json::array();
		common::add_at_message(message, user_id);
		common::add_text_message(message, " 欢迎喵~爱你喵~");
		common::send_group_msg(api, group_id, message);
	}
	else if (notice_type == "group_ban") {
	}
	else if (notice_type == "friend_add") {
	}
	else if (notice_type == "group_recall") {
	}
	else if (notice_type == "friend_recall") {
	}
	else if (notice_type == "notify") {
	}
}