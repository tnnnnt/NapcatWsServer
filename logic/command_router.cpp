#include "command_router.h"
#include "common.hpp"
#include "message/handle_message.h"
#include "notice/handle_notice.h"
#include <fstream>
#include <iostream>
#include <mutex>
#include <string>

void CommandRouter::handle(const json& event, ApiFunc api) {
	const std::string post_type = event.at("post_type").get<std::string>();
	if (post_type == "message") HandleMessage::start(event, api);
	else if (post_type == "notice") HandleNotice::start(event, api);
	else if (post_type == "request") {}
	else if (post_type == "meta_event") {}
}
void CommandRouter::daily(ApiFunc api) {
	updata_group_members_data(api);
	{
		std::lock_guard<std::mutex> lock(common::today_group_member_message_number_mutex);
		common::today_group_member_message_number.clear();
	}
}
void CommandRouter::load_config() {
	std::ifstream ifs(common::CONFIG_FILE);
	if (!ifs.is_open()) {
		std::cerr << "无法打开配置文件: " << common::CONFIG_FILE << std::endl;
		return;
	}
	json config_json;
	ifs >> config_json;
	common::ADMIN_QQ = config_json.at("admin_qq").get<int64_t>();
	common::ROBOT_QQ = config_json.at("robot_qq").get<int64_t>();
	common::POOL_SIZE = config_json.at("pool_size").get<size_t>();
	common::RANK_SIZE = config_json.at("rank_size").get<size_t>();
	common::BASE_DELAY = config_json.at("base_delay").get<size_t>();
	common::RANDOM_DELAY = config_json.at("random_delay").get<size_t>();
	common::TIME_SAVE_INTERVAL = config_json.at("time_save_interval").get<size_t>();
	common::TIME_ZONE_OFFSET = config_json.at("time_zone_offset").get<size_t>();
	common::EAT_DIR = common::WORK_DIR + config_json.at("eat_dir").get<std::string>();
	common::DRINK_DIR = common::WORK_DIR + config_json.at("drink_dir").get<std::string>();
	common::TODAY_GROUP_MEMBER_MESSAGE_NUMBER_FILE = common::SAVE_DIR + config_json.at("today_group_member_message_number_file").get<std::string>();
}
void CommandRouter::updata_group_members_data(ApiFunc api) {
	std::lock_guard<std::mutex> lock(common::group_members_mutex);
	common::group_members.clear();
	json params{};
	auto a = api("get_group_list", params);
	const auto group_list_data = a["data"];
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
void CommandRouter::save_today_group_member_message_number_data() {
	std::lock_guard<std::mutex> lock(common::today_group_member_message_number_mutex);
	json j;
	for (const auto& group_pair : common::today_group_member_message_number)
	{
		const std::string group_id = std::to_string(group_pair.first);
		for (const auto& user_pair : group_pair.second)
		{
			const std::string user_id = std::to_string(user_pair.first);
			j[group_id][user_id] = user_pair.second;
		}
	}
	std::ofstream ofs(common::TODAY_GROUP_MEMBER_MESSAGE_NUMBER_FILE);
	ofs << j.dump();
}
void CommandRouter::load_today_group_member_message_number_data() {
	std::lock_guard<std::mutex> lock(common::today_group_member_message_number_mutex);
	json j;
	std::ifstream ifs(common::TODAY_GROUP_MEMBER_MESSAGE_NUMBER_FILE);
	ifs >> j;
	for (auto& [group_id_str, users] : j.items())
	{
		int64_t group_id = std::stoll(group_id_str);
		for (auto& [user_id_str, count] : users.items())
		{
			int64_t user_id = std::stoll(user_id_str);
			common::today_group_member_message_number[group_id][user_id] = count.get<int>();
		}
	}
}