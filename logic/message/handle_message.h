#pragma once
#include "../logic_types.hpp"
class HandleMessage {
  public:
	static void start(const json& event, const ApiFunc& api);

  private:
	static void filter_valid_messages(json& message_array);
	static void handle_eat_drink(const ApiFunc& api, int64_t group_id, int64_t time, const std::string& raw_message);
	static void
	handle_today_fortune(const ApiFunc& api, int64_t group_id, int64_t user_id, int luckey_num, int64_t seed);
	static void handle_today_relation(
		const ApiFunc& api, int64_t group_id, int64_t user_id, int luckey_num, int64_t seed, const std::string& text);
	static void handle_relation_graph(const ApiFunc& api, int64_t group_id);
	static void handle_message_rank(const ApiFunc& api, int64_t group_id);
	static void handle_subscribe(const ApiFunc& api, int64_t group_id, int64_t user_id);
	static void handle_unsubscribe(const ApiFunc& api, int64_t group_id, int64_t user_id);
	static void handle_get_sex_image(const ApiFunc& api, int64_t group_id, int64_t time);
	static void handle_oral_edict(const ApiFunc& api, int64_t group_id, int64_t user_id, const std::string& text);
	static void handle_at_robot(const ApiFunc& api, int64_t group_id);
	static void
	handle_upload_sex_image(const ApiFunc& api, int64_t group_id, int64_t user_id, const std::string& message_id);
	static void handle_upload_eat(
		const ApiFunc& api, int64_t group_id, int64_t user_id, const std::string& word, const std::string& message_id);
	static void handle_upload_drink(
		const ApiFunc& api, int64_t group_id, int64_t user_id, const std::string& word, const std::string& message_id);
	static void
	handle_ban(const ApiFunc& api, int64_t group_id, int64_t user_id, const std::string& type1, const json& seg_obj1);
	static void
	handle_allow(const ApiFunc& api, int64_t group_id, int64_t user_id, const std::string& type1, const json& seg_obj1);
};