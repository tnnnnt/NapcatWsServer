#include <algorithm>
#include <filesystem>
#include <string>
#include <vector>
#include "command_router.h"
#include "message_utils.hpp"
inline const std::string WORK_DIR = "/home/bot/qq_robot/"; // 工作目录
inline const std::string SEX_DIR = WORK_DIR + "sex/";	   // 色图
const std::vector<std::string> SEX_FORWARD_COMMAND_KEYWORDS = {"收", "嗯嗯", "上传相册"}; // 触发「转发色图」的关键词
constexpr int64_t TEST_GROUP = 1092859942;	 // 测试群号，仅该群触发回显，避免打扰其他群
constexpr int64_t SEX_GROUP = 424933468;	 // 色图审核群号
constexpr int64_t YUN_ROBOT_QQ = 2444570668; // 小云机器人QQ

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

static bool is_command_of_forward_sex_image(const std::string& raw_message, const json& message_array) {
	// 触发条件一：消息含「上传色图」字样
	if (raw_message.find("上传色图") != std::string::npos) {
		return true;
	}
	// 触发条件二：同时满足「含转发关键词」且「@ 了小云机器人」
	bool has_forward_command = false;
	bool has_call_yun = false;
	for (const auto& msg : message_array) {
		const auto& type = msg.at("type");
		if (type == "text") {
			const auto& text = msg.at("data").at("text").get<std::string>();
			for (const auto& keyword : SEX_FORWARD_COMMAND_KEYWORDS) {
				has_forward_command |= text.find(keyword) != std::string::npos;
			}
			has_call_yun |= text.find("小云") != std::string::npos;
		}
		else if (type == "at") {
			has_call_yun |= std::stoll(msg.at("data").at("qq").get<std::string>()) == YUN_ROBOT_QQ;
		}
		if (has_forward_command && has_call_yun) {
			return true;
		}
	}
	return false;
}

// 转发色图：把带图消息（或回复引用的消息）原样转发到审核群 SEX_GROUP 供人工审核。
// 注意：这里不是「下载图片到本地」，而是「转发消息到另一个群」。
static void handle_forward_sex_image(const ApiFunc& api, const json& message_array, int64_t group_id) {
	bool suc = false;
	if (message_array.empty()) {
		json message = json::array();
		msg::add_text_message(message, "没有可上传的图片喵~");
		msg::send_group_msg(api, group_id, message);
		return;
	}
	const auto& type = message_array[0].at("type");
	if (type == "reply") {
		// 回复消息：先取被回复的那条消息，再转发到审核群
		json params{{"message_id", message_array[0].at("data").at("id")}};
		const auto message = api("get_msg", params).at("data").at("message");
		suc = msg::send_group_msg(api, SEX_GROUP, message).at("retcode").get<int64_t>() == 0;
	}
	else {
		// 直接消息：原样转发到审核群
		suc = msg::send_group_msg(api, SEX_GROUP, message_array).at("retcode").get<int64_t>() == 0;
	}
	json message = json::array();
	msg::add_text_message(message, std::string("上传") + (suc ? "成功！" : "失败！"));
	msg::send_group_msg(api, group_id, message);
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

		// 转发色图到审核群
		if (is_command_of_forward_sex_image(raw_message, message_array)) {
			handle_forward_sex_image(api, message_array, group_id);
			return;
		}
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
