#include "command_router.h"
#include <fstream>
#include <iostream>
#include <mutex>
#include <string>
#include "common.hpp"
#include "message/handle_message.h"
#include "notice/handle_notice.h"

void CommandRouter::handle(const json& event, const ApiFunc& api) {
	const std::string post_type = event.at("post_type").get<std::string>();
	if (post_type == "message") {
		HandleMessage::start(event, api);
	}
	else if (post_type == "notice") {
		HandleNotice::start(event, api);
	}
	else if (post_type == "request") {
	}
	else if (post_type == "meta_event") {
	}
}
void CommandRouter::daily(const ApiFunc& api) {
	load_config_daily();
	load_fortune();
	updata_group_members_data(api);
	{
		std::lock_guard<std::mutex> lock(common::group_member_relations_mutex);
		common::group_member_relations.clear();
	}
}
void CommandRouter::load_config_static() {
	common::load_json_config(common::CONFIG_STATIC_FILE, common::CONFIG_STATIC);
}
void CommandRouter::load_config_periodic() {
	common::load_json_config(common::CONFIG_PERIODIC_FILE, common::CONFIG_PERIODIC);
}
void CommandRouter::load_config_daily() {
	common::load_json_config(common::CONFIG_DAILY_FILE, common::CONFIG_DAILY);
}
void CommandRouter::load_fortune() {
	common::fortunes = common::read_file_lines(common::FORTUNE_FILE);
}
void CommandRouter::load_sex_upload_commond_keyword() {
	common::sex_upload_commond_keywords = common::read_file_lines(common::SEX_UPLOAD_COMMOND_KEYWORD_FILE);
}
void CommandRouter::updata_group_members_data(const ApiFunc& api) {
	std::lock_guard<std::mutex> lock(common::group_members_mutex);
	common::group_ids.clear();
	common::group_members.clear();
	common::group_active_members.clear();
	json params{};
	auto a = api("get_group_list", params);
	const auto group_list_data = a["data"];
	for (const auto& group : group_list_data) {
		const int64_t group_id = group.at("group_id").get<int64_t>();
		const int64_t member_count = group.at("member_count").get<int64_t>();
		common::group_ids.push_back(group_id);
		common::group_members[group_id].reserve(member_count);
		common::group_active_members[group_id].reserve(member_count);
		params["group_id"] = group_id;
		const auto group_member_list_data = api("get_group_member_list", params)["data"];
		for (const auto& member : group_member_list_data) {
			const int64_t user_id = member.at("user_id").get<int64_t>();
			common::group_members[group_id].push_back(user_id);
			params["user_id"] = user_id;
			const size_t level =
				std::stoul(api("get_group_member_info", params)["data"].at("level").get<std::string>());
			if (level >= common::CONFIG_DAILY.min_activity_level) {
				common::group_active_members[group_id].push_back(user_id);
			}
		}
	}
}
void CommandRouter::save_notice_group_member_data() {
	std::ofstream ofs(common::NOTICE_GROUP_MEMBER_FILE);
	ofs << common::notice_group_member_json.dump();
}
void CommandRouter::load_notice_group_member_data() {
	std::ifstream ifs(common::NOTICE_GROUP_MEMBER_FILE);
	ifs >> common::notice_group_member_json;
}