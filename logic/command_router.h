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
};