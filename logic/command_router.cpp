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
	else if (post_type == "request"){}
	else if (post_type == "meta_event"){}
}
void CommandRouter::daily(ApiFunc api) {
	{
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
	{
		std::lock_guard<std::mutex> lock(common::group_member_message_number_mutex);
		common::yesterday_group_member_message_rank.clear();
		common::yesterday_group_member_message_number = std::move(common::today_group_member_message_number);
		for (const auto& it : common::yesterday_group_member_message_number) {
			const int64_t group_id = it.first;
			const auto& member_message_number = it.second;
			auto& member_message_rank = common::yesterday_group_member_message_rank[group_id];
			member_message_rank.insert(member_message_rank.end(), member_message_number.begin(), member_message_number.end());
			const int k = std::min(common::RANK_SIZE, member_message_rank.size());
			std::partial_sort(
				member_message_rank.begin(), member_message_rank.begin() + k, member_message_rank.end(),
				[](const std::pair<int64_t, int>& a, const std::pair<int64_t, int>& b)
				{
					return a.second > b.second; // 按 value 降序
				});
		}
	}
}