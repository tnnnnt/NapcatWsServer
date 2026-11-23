#include <algorithm>
#include <filesystem>
#include <string>
#include <vector>
#include "command_router.h"
#include "message_utils.hpp"
inline const std::string WORK_DIR = "/home/bot/qq_robot/"; // 工作目录
inline const std::string SEX_DIR = WORK_DIR + "sex/";	   // 色图
constexpr int64_t TEST_GROUP = 1092859942;				   // 测试群号，仅该群触发回显，避免打扰其他群

static void get_files(const std::filesystem::path& dir_path, std::vector<std::string>& files) {
	files.clear();
	if (!std::filesystem::exists(dir_path) || !std::filesystem::is_directory(dir_path)) {
		return;
	}
	for (const auto& entry : std::filesystem::directory_iterator(dir_path)) {
		if (entry.is_regular_file()) {
			files.push_back(entry.path().filename());
		}
	}
}

static void handle_get_sex_image(const ApiFunc& api, int64_t group_id, int64_t time) {
	std::vector<std::string> files;
	get_files(SEX_DIR, files);
	json message = json::array();
	if (files.empty()) {
		msg::add_text_message(message, "没有色图了喵~");
	}
	else {
		const std::string random_file = files[time % files.size()];
		msg::add_image_message(message, "file:///" + SEX_DIR + random_file);
	}
	msg::send_group_msg(api, group_id, message);
}

static void trim(std::string& str) {
	const auto first = str.find_first_not_of(" \t\n\r\f\v");
	if (first == std::string::npos) {
		str.clear();
		return;
	}
	const auto last = str.find_last_not_of(" \t\n\r\f\v");
	str.erase(last + 1);
	str.erase(0, first);
}

static void filter_valid_messages(json& message_array) {
	message_array.erase(std::remove_if(message_array.begin(),
									   message_array.end(),
									   [](json& message) {
										   if (message["type"] != "text") {
											   return false;
										   }
										   auto& text = message["data"]["text"].get_ref<std::string&>();
										   trim(text);
										   return text.empty();
									   }),
						message_array.end());
}

static void handle_message(const json& event, const ApiFunc& api) {
	const std::string message_type = event.at("message_type").get<std::string>();
	if (message_type == "private") {
		// 私聊处理
	}
	else if (message_type == "group") {
		const int64_t group_id = event.at("group_id").get<int64_t>();
		// 测试回显
		if (group_id == TEST_GROUP) {
			json message = json::array();
			msg::add_text_message(message, event.dump(4));
			msg::send_group_msg(api, group_id, message);
		}

		const auto time = event.at("time").get<int64_t>() + 28800; // 转为北京时间
		const std::string raw_message = event.at("raw_message").get<std::string>();
		if (raw_message == "来点色图") {
			handle_get_sex_image(api, group_id, time);
			return;
		}

		// 预处理
		json message_array = event.at("message");
		filter_valid_messages(message_array);

	}
}

static void handle_notice(const json& event, const ApiFunc& api) {
	// todo
}

void CommandRouter::handle(const json& event, const ApiFunc& api) {
	const std::string post_type = event.at("post_type").get<std::string>();
	if (post_type == "message") {
		handle_message(event, api);
	}
	else if (post_type == "notice") {
		handle_notice(event, api);
	}
}
