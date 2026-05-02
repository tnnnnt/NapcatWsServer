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
	load_config();
	updata_group_members_data(api);
	for (const int64_t& group_id : common::group_ids) {
		send_today_group_member_message_number_data(group_id, api);
	}
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
	common::MIN_ACTIVITY_LEVEL = config_json.at("min_activity_level").get<size_t>();
}
void CommandRouter::updata_group_members_data(ApiFunc api) {
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
			const size_t level = std::stoul(api("get_group_member_info", params)["data"].at("level").get<std::string>());
			if (level >= common::MIN_ACTIVITY_LEVEL) {
				common::group_active_members[group_id].push_back(user_id);
			}
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
void CommandRouter::del_today_group_member_message_number_data(const int64_t& group_id, const int64_t& user_id) {
	std::lock_guard<std::mutex> lock(common::today_group_member_message_number_mutex);
	common::today_group_member_message_number[group_id].erase(user_id);
}
void CommandRouter::send_today_group_member_message_number_data(const int64_t& group_id, ApiFunc api) {
	json params{};
	params["group_id"] = group_id;
	json message = json::array();
	common::add_text_message(message, "今日龙王榜\n\n");
	std::vector<std::pair<int64_t, int>>member_message_rank;
	{
		std::lock_guard<std::mutex> lock(common::today_group_member_message_number_mutex);
		const auto& member_message_number = common::today_group_member_message_number[group_id];
		member_message_rank.insert(member_message_rank.end(), member_message_number.begin(), member_message_number.end());
	}
	const size_t k = std::min(common::RANK_SIZE, member_message_rank.size());
	std::partial_sort(
		member_message_rank.begin(), member_message_rank.begin() + k, member_message_rank.end(),
		[](const std::pair<int64_t, int>& a, const std::pair<int64_t, int>& b)
		{
			return a.second > b.second; // 按 value 降序
		});
	for (size_t i = 0; i < k; ++i) {
		const int64_t user_id = member_message_rank[i].first;
		const int message_num = member_message_rank[i].second;
		const auto group_member_info = api("get_group_member_info", json{ {"group_id", std::to_string(group_id)}, { "user_id", std::to_string(user_id) } })["data"];
		const std::string card = group_member_info.at("card").get<std::string>();
		const std::string user_name = card == "" ? group_member_info.at("nickname").get<std::string>() : card;
		common::add_text_message(message, std::to_string(i + 1) + ". " + user_name + " 发言数：" + std::to_string(message_num) + "\n");
		common::add_image_message(message, "https://q.qlogo.cn/g?b=qq&nk=" + std::to_string(user_id) + "&s=1");
	}
	params["message"] = message;
	api("send_group_msg", params);
}
void CommandRouter::save_notice_group_member_data(json& j) {
	std::ofstream ofs(common::NOTICE_GROUP_MEMBER_FILE);
	ofs << j.dump();
}
void CommandRouter::load_notice_group_member_data(json& j) {
	std::ifstream ifs(common::NOTICE_GROUP_MEMBER_FILE);
	ifs >> j;
}
void CommandRouter::add_notice_group_member(const int64_t& group_id, const int64_t& user_id) {
	json j;
	load_notice_group_member_data(j);
	const std::string group_id_str = std::to_string(group_id);
	const std::string user_id_str = std::to_string(user_id);
	if (!j.contains(group_id_str)) {
		j[group_id_str] = json::array();
	}
	auto& members = j[group_id_str];
	if (std::find(members.begin(), members.end(), user_id_str) == members.end()) {
		members.push_back(user_id_str);
		save_notice_group_member_data(j);
	}
}
void CommandRouter::del_notice_group_member(const int64_t& group_id, const int64_t& user_id) {
	json j;
	load_notice_group_member_data(j);
	const std::string group_id_str = std::to_string(group_id);
	const std::string user_id_str = std::to_string(user_id);
	if (j.contains(group_id_str)) {
		auto& members = j[group_id_str];
		members.erase(std::remove(members.begin(), members.end(), user_id_str), members.end());
		save_notice_group_member_data(j);
	}
}
void CommandRouter::get_notice_members_by_group(const int64_t& group_id, std::vector<int64_t>& user_ids) {
	json j;
	load_notice_group_member_data(j);
	const std::string group_id_str = std::to_string(group_id);
	if (j.contains(group_id_str)) {
		const auto& members = j[group_id_str];
		for (const auto& user_id_str : members) {
			user_ids.push_back(std::stoll(user_id_str.get<std::string>()));
		}
	}
}