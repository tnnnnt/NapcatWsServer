#include "command_router.h"
#include "common.hpp"
#include "message/handle_message.h"
#include "notice/handle_notice.h"
#include <iostream>
#include <mutex>
#include <string>

void CommandRouter::handle(const json& event, ApiFunc api) {
	const std::string post_type = event.at("post_type").get<std::string>();
	if (post_type == "message") HandleMessage::start(event, api);
	else if (post_type == "notice") HandleNotice::start(event, api);
	else if (post_type == "request") ;
	else if (post_type == "meta_event") ;
}
void CommandRouter::daily(ApiFunc api) {
	std::lock_guard<std::mutex> lock(common::group_members_mutex);
	common::group_members.clear();
	json params{};
	const auto group_list_data = api("get_group_list", params)["data"];
	for (const auto& group : group_list_data) {
		const int64_t group_id = group.at("group_id").get<int64_t>();
		const int64_t member_count = group.at("member_count").get<int64_t>();
		common::group_members[group_id].reserve(member_count);
		params["group_id"] = group_id;
		const auto group_member_list_data = api("get_group_member_list", params)["data"];
		for (const auto& member : group_member_list_data) {
			const int64_t user_id = member.at("user_id").get<int64_t>();
			common::group_members[group_id].push_back(user_id);
		}
	}
}