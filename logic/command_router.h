#pragma once
#include "logic_types.hpp"
class CommandRouter {
  public:
	static void handle(const json& event, const ApiFunc& api);
	static void daily(const ApiFunc& api);
	static void load_config();
	static void updata_group_members_data(const ApiFunc& api);
	static void save_today_group_member_message_number_data();
	static void load_today_group_member_message_number_data();
	static void del_today_group_member_message_number_data(int64_t group_id, int64_t user_id);
	static void send_today_group_member_message_number_data(int64_t group_id, const ApiFunc& api);
	static void save_notice_group_member_data(json& j);
	static void load_notice_group_member_data(json& j);
	static void add_notice_group_member(int64_t group_id, int64_t user_id);
	static void del_notice_group_member(int64_t group_id, int64_t user_id);
	static void get_notice_members_by_group(int64_t group_id, std::vector<int64_t>& user_ids);
};