#pragma once
#include <nlohmann/json.hpp>
#include <functional>

using json = nlohmann::json;

class CommandRouter {
public:
	using ApiFunc = std::function<json(const std::string&, const json&)>;

	static void handle(const json& event, ApiFunc api);
	static void daily(ApiFunc api);
	static void load_config();
	static void updata_group_members_data(ApiFunc api);
	static void save_today_group_member_message_number_data();
	static void load_today_group_member_message_number_data();
	static void del_today_group_member_message_number_data(const int64_t& group_id, const int64_t& user_id);
	static void send_today_group_member_message_number_data(const int64_t& group_id, ApiFunc api);
	static void save_notice_group_member_data(json& j);
	static void load_notice_group_member_data(json& j);
	static void add_notice_group_member(const int64_t& group_id, const int64_t& user_id);
	static void del_notice_group_member(const int64_t& group_id, const int64_t& user_id);
	static void get_notice_members_by_group(const int64_t& group_id, std::vector<int64_t>& user_ids);
};