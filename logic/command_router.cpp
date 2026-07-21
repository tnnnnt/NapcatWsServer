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
	for (const int64_t& group_id : common::group_ids) {
		send_today_group_member_message_number_data(group_id, api);
	}
	{
		std::lock_guard<std::mutex> lock(common::today_group_member_message_number_mutex);
		common::today_group_member_message_number.clear();
	}
	{
		std::lock_guard<std::mutex> lock(common::group_member_relations_mutex);
		common::group_member_relations.clear();
	}
}
void CommandRouter::load_config_static() {
	std::ifstream ifs(common::CONFIG_STATIC_FILE);
	if (!ifs.is_open()) {
		std::cerr << "无法打开配置文件: " << common::CONFIG_STATIC_FILE << std::endl;
		return;
	}
	common::CONFIG_STATIC = json::parse(ifs).get<common::ConfigStatic>();
}
void CommandRouter::load_config_periodic() {
	std::ifstream ifs(common::CONFIG_PERIODIC_FILE);
	if (!ifs.is_open()) {
		std::cerr << "无法打开配置文件: " << common::CONFIG_PERIODIC_FILE << std::endl;
		return;
	}
	common::CONFIG_PERIODIC = json::parse(ifs).get<common::ConfigPeriodic>();
}
void CommandRouter::load_config_daily() {
	std::ifstream ifs(common::CONFIG_DAILY_FILE);
	if (!ifs.is_open()) {
		std::cerr << "无法打开配置文件: " << common::CONFIG_DAILY_FILE << std::endl;
		return;
	}
	common::CONFIG_DAILY = json::parse(ifs).get<common::ConfigDaily>();
}
void CommandRouter::load_fortune() {
	std::ifstream ifs_fortune(common::FORTUNE_FILE);
	if (!ifs_fortune.is_open()) {
		std::cerr << "无法打开运势文件: " << common::FORTUNE_FILE << std::endl;
		return;
	}
	std::string fortune;
	common::fortunes.clear();
	while (std::getline(ifs_fortune, fortune)) {
		common::fortunes.push_back(fortune);
	}
}
void CommandRouter::load_sex_upload_commond_keyword() {
	std::ifstream ifs_sex_upload_commond_keyword(common::SEX_UPLOAD_COMMOND_KEYWORD_FILE);
	if (!ifs_sex_upload_commond_keyword.is_open()) {
		std::cerr << "无法打开上传色图命令关键字文件: " << common::SEX_UPLOAD_COMMOND_KEYWORD_FILE << std::endl;
		return;
	}
	std::string sex_upload_commond_keyword;
	while (std::getline(ifs_sex_upload_commond_keyword, sex_upload_commond_keyword)) {
		common::sex_upload_commond_keywords.push_back(sex_upload_commond_keyword);
	}
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
void CommandRouter::save_today_group_member_message_number_data() {
	std::lock_guard<std::mutex> lock(common::today_group_member_message_number_mutex);
	json j{};
	for (const auto& group_pair : common::today_group_member_message_number) {
		const std::string group_id = std::to_string(group_pair.first);
		for (const auto& user_pair : group_pair.second) {
			const std::string user_id = std::to_string(user_pair.first);
			j[group_id][user_id] = user_pair.second;
		}
	}
	std::ofstream ofs(common::TODAY_GROUP_MEMBER_MESSAGE_NUMBER_FILE);
	ofs << j.dump();
}
void CommandRouter::load_today_group_member_message_number_data() {
	std::lock_guard<std::mutex> lock(common::today_group_member_message_number_mutex);
	json j{};
	std::ifstream ifs(common::TODAY_GROUP_MEMBER_MESSAGE_NUMBER_FILE);
	ifs >> j;
	for (auto& [group_id_str, users] : j.items()) {
		int64_t group_id = std::stoll(group_id_str);
		for (auto& [user_id_str, count] : users.items()) {
			int64_t user_id = std::stoll(user_id_str);
			common::today_group_member_message_number[group_id][user_id] = count.get<int>();
		}
	}
}
void CommandRouter::del_today_group_member_message_number_data(int64_t group_id, int64_t user_id) {
	std::lock_guard<std::mutex> lock(common::today_group_member_message_number_mutex);
	common::today_group_member_message_number[group_id].erase(user_id);
}
void CommandRouter::send_today_group_member_message_number_data(int64_t group_id, const ApiFunc& api) {
	json params{};
	params["group_id"] = group_id;
	json message = json::array();
	common::add_text_message(message, "今日龙王榜\n\n");
	std::vector<std::pair<int64_t, int>> member_message_rank;
	{
		std::lock_guard<std::mutex> lock(common::today_group_member_message_number_mutex);
		const auto& member_message_number = common::today_group_member_message_number[group_id];
		member_message_rank.insert(
			member_message_rank.end(), member_message_number.begin(), member_message_number.end());
	}
	const size_t k = std::min(common::CONFIG_STATIC.rank_size, member_message_rank.size());
	std::partial_sort(member_message_rank.begin(),
					  member_message_rank.begin() + k,
					  member_message_rank.end(),
					  [](const std::pair<int64_t, int>& a, const std::pair<int64_t, int>& b) {
						  return a.second > b.second; // 按 value 降序
					  });
	for (size_t i = 0; i < k; ++i) {
		const int64_t user_id = member_message_rank[i].first;
		const int message_num = member_message_rank[i].second;
		std::string user_name;
		common::get_group_member_name(api, group_id, user_id, user_name);
		common::add_text_message(
			message, std::to_string(i + 1) + ". " + user_name + " 发言数：" + std::to_string(message_num) + "\n");
		common::add_image_message(message, "https://q.qlogo.cn/g?b=qq&nk=" + std::to_string(user_id) + "&s=1");
	}
	params["message"] = message;
	api("send_group_msg", params);
}
void CommandRouter::save_notice_group_member_data() {
	std::ofstream ofs(common::NOTICE_GROUP_MEMBER_FILE);
	ofs << common::notice_group_member_json.dump();
}
void CommandRouter::load_notice_group_member_data() {
	std::ifstream ifs(common::NOTICE_GROUP_MEMBER_FILE);
	ifs >> common::notice_group_member_json;
}
void CommandRouter::load_ban_data() {
	std::ifstream ifs_ban(common::BAN_FILE);
	if (!ifs_ban.is_open()) {
		std::cerr << "无法打开BAN文件: " << common::BAN_FILE << std::endl;
		return;
	}
	std::string ban;
	while (std::getline(ifs_ban, ban)) {
		common::bans.insert(ban);
	}
}
void CommandRouter::save_ban_data() {
	std::ofstream ofs_ban(common::BAN_FILE);
	for (const auto& ban : common::bans) {
		ofs_ban << ban << "\n";
	}
}