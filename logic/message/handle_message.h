#pragma once
#include "../logic_types.hpp"
class HandleMessage {
  public:
	static void start(const json& event, const ApiFunc& api);

  private:
	static void handle_eat_drink(const ApiFunc& api, int64_t group_id, int64_t time, const std::string& raw_message);
	static void handle_today_fortune(const ApiFunc& api, int64_t group_id, int64_t user_id, int64_t seed);
	static void
	handle_today_relation(const ApiFunc& api, int64_t group_id, int64_t user_id, int64_t seed, const std::string& text);
	static void handle_relation_graph(const ApiFunc& api, int64_t group_id);
	static void handle_subscribe(const ApiFunc& api, int64_t group_id, int64_t user_id);
	static void handle_unsubscribe(const ApiFunc& api, int64_t group_id, int64_t user_id);
	static void handle_get_sex_image(const ApiFunc& api, int64_t group_id, int64_t time);
	static void filter_valid_messages(json& message_array);
	static bool is_commond_of_upload_sex_image(const std::string& raw_message, const json& message_array);
	static void handle_upload_sex_image(const ApiFunc& api, const json& message_array, int64_t group_id);
	static bool
	upload_eat_or_drink_image(const ApiFunc& api, const json& message_array, int64_t group_id, std::string& word);
	static void handle_upload_eat_or_drink_image(
		const ApiFunc& api, const json& message_array, const std::string& word, int64_t group_id, bool is_eat);
	static void download_sex_images(
		const ApiFunc& api, const json& messages, const std::string& save_dir, int& suc, int& total, json& message);
	static void download_sex_image(
		const ApiFunc& api, const json& image_data, const std::string& save_dir, int& suc, int& total, json& message);
};