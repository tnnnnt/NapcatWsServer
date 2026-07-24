#include "handle_message.h"
#include <fstream>
#include <string>
#include <vector>
#include "../command_router.h"
#include "../common.hpp"

void HandleMessage::start(const json& event, const ApiFunc& api) {
	const std::string message_type = event.at("message_type").get<std::string>();
	if (message_type == "private") {
	}
	else if (message_type == "group") {
		const int64_t group_id = event.at("group_id").get<int64_t>();
		if (group_id == common::CONFIG_STATIC.test_group) {
			json params{};
			params["group_id"] = group_id;
			json message = json::array();
			common::add_text_message(message, event.dump(4));
			params["message"] = message;
			api("send_group_msg", params);
		}
		const int64_t user_id = event.at("user_id").get<int64_t>();
		const auto time = event.at("time").get<int64_t>() + 28800; // 转为北京时间
		const std::string raw_message = event.at("raw_message").get<std::string>();

		if (raw_message.find("吃什么") != std::string::npos || raw_message.find("吃啥") != std::string::npos ||
			raw_message.find("喝什么") != std::string::npos || raw_message.find("喝啥") != std::string::npos) {
			handle_eat_drink(api, group_id, time, raw_message);
			return;
		}

		const int64_t seed = time / 86400 + user_id; // 每天每人一个固定的 seed
		if (raw_message == "今日运势") {
			handle_today_fortune(api, group_id, user_id, seed);
			return;
		}
		if (raw_message == "今日老婆" || raw_message == "今日老公" || raw_message == "今日妈妈" ||
			raw_message == "今日主人") {
			handle_today_relation(api, group_id, user_id, seed, raw_message);
			return;
		}
		if (raw_message == "关系图") {
			handle_relation_graph(api, group_id);
			return;
		}
		if (raw_message == "订阅通知") {
			handle_subscribe(api, group_id, user_id);
			return;
		}
		if (raw_message == "取消订阅") {
			handle_unsubscribe(api, group_id, user_id);
			return;
		}
		if (raw_message == "来点色图") {
			handle_get_sex_image(api, group_id, time);
			return;
		}

		json message_array = event.at("message");
		filter_valid_messages(message_array);
		if (is_commond_of_upload_sex_image(raw_message, message_array)) {
			handle_upload_sex_image(api, message_array, group_id, user_id);
			return;
		}
		std::string word;
		if (upload_eat_or_drink_image(api, message_array, group_id, word)) {
			return;
		}
		if (raw_message.find("banbanban") != std::string::npos) {
			handle_ban_or_allow(api, message_array, group_id, user_id, true);
			return;
		}
		if (raw_message.find("allow") != std::string::npos) {
			handle_ban_or_allow(api, message_array, group_id, user_id, false);
			return;
		}
	}
}
void HandleMessage::handle_eat_drink(const ApiFunc& api,
									 int64_t group_id,
									 int64_t time,
									 const std::string& raw_message) {
	const bool is_eat = raw_message.find("吃") != std::string::npos;
	const auto& dir = is_eat ? common::EAT_DIR : common::DRINK_DIR;
	std::vector<std::string> files;
	common::get_files(dir, files);
	json params{};
	params["group_id"] = group_id;
	json message = json::array();
	const std::string random_file = files[time % files.size()];
	common::add_text_message(message, "推荐" + common::remove_extension(random_file));
	common::add_image_message(message, "file:///" + dir + random_file);
	params["message"] = message;
	api("send_group_msg", params);
}
void HandleMessage::handle_today_fortune(const ApiFunc& api, int64_t group_id, int64_t user_id, int64_t seed) {
	json params{};
	params["group_id"] = group_id;
	json message = json::array();
	int luckey_num = 0;
	common::shuffle_vector(common::fortunes, luckey_num, seed);
	common::add_at_message(message, user_id);
	common::add_text_message(message,
							 "\n今日运势（仅供娱乐）\n宜：" + common::fortunes[0] + " " + common::fortunes[1] + " " +
								 common::fortunes[2] + "\n忌：" + common::fortunes[3] + " " + common::fortunes[4] +
								 " " + common::fortunes[5] + "\n幸运数字：" + std::to_string(luckey_num));
	params["message"] = message;
	api("send_group_msg", params);
}
void HandleMessage::handle_today_relation(
	const ApiFunc& api, int64_t group_id, int64_t user_id, int64_t seed, const std::string& text) {
	std::vector<int64_t> temp;
	{
		std::lock_guard<std::mutex> lock(common::group_members_mutex);
		temp = (common::group_active_members[group_id].empty() ? common::group_members
															   : common::group_active_members)[group_id];
	}
	int luckey_num = 0;
	common::shuffle_vector(temp, luckey_num, seed);
	const int member_count = temp.size();
	int64_t match_id = 0;
	{
		std::lock_guard<std::mutex> lock(common::group_member_relations_mutex);
		auto& member_relations = common::group_member_relations[group_id];
		if (text == "今日老婆") {
			if (member_relations[user_id].wife_id) {
				match_id = member_relations[user_id].wife_id;
			}
			else {
				match_id = member_relations[user_id].wife_id = temp[0];
			}
		}
		else if (text == "今日老公") {
			if (member_relations[user_id].husband_id) {
				match_id = member_relations[user_id].husband_id;
			}
			else {
				match_id = member_relations[user_id].husband_id = temp[std::min(member_count - 1, 1)];
			}
		}
		else if (text == "今日妈妈") {
			if (member_relations[user_id].mother_id) {
				match_id = member_relations[user_id].mother_id;
			}
			else {
				match_id = member_relations[user_id].mother_id = temp[std::min(member_count - 1, 2)];
			}
		}
		else if (text == "今日主人") {
			if (member_relations[user_id].master_id) {
				match_id = member_relations[user_id].master_id;
			}
			else {
				match_id = member_relations[user_id].master_id = temp[std::min(member_count - 1, 3)];
			}
		}
	}
	std::string match_name;
	common::get_group_member_name(api, group_id, match_id, match_name);
	json params{};
	params["group_id"] = group_id;
	json message = json::array();
	common::add_at_message(message, user_id);
	std::string last_sentence;
	if (text == "今日老婆") {
		last_sentence = "请好好对待她哦~";
	}
	else if (text == "今日老公") {
		last_sentence = "请好好对待他哦~";
	}
	else if (text == "今日妈妈") {
		last_sentence = "快去叫妈妈！";
	}
	else if (text == "今日主人") {
		last_sentence = "快去叫主人！";
	}
	common::add_text_message(message, "\n你的" + text + "是【" + match_name + "】\n" + last_sentence);
	common::add_image_message(message, "https://q.qlogo.cn/g?b=qq&nk=" + std::to_string(match_id) + "&s=4");
	params["message"] = message;
	api("send_group_msg", params);
}
void HandleMessage::handle_relation_graph(const ApiFunc& api, int64_t group_id) {
	const std::string file_name = common::RELATION_DIR + std::to_string(group_id) + ".png";
	{
		std::lock_guard<std::mutex> lock(common::group_member_relations_mutex);
		json qq_name_json{};
		json relations_json{};
		for (const auto& member_relations : common::group_member_relations[group_id]) {
			const int64_t member = member_relations.first;
			const std::string member_str = std::to_string(member);
			std::string name;
			if (!qq_name_json.count(member_str)) {
				common::get_group_member_name(api, group_id, member, name);
				qq_name_json[member_str] = name;
			}
			const common::Relation relations = member_relations.second;
			const int64_t husband_id = relations.husband_id;
			const int64_t master_id = relations.master_id;
			const int64_t mother_id = relations.mother_id;
			const int64_t wife_id = relations.wife_id;
			if (husband_id) {
				const std::string husband_id_str = std::to_string(husband_id);
				relations_json[member_str]["husband"] = husband_id_str;
				if (!qq_name_json.count(husband_id_str)) {
					common::get_group_member_name(api, group_id, husband_id, name);
					qq_name_json[husband_id_str] = name;
				}
			}
			if (master_id) {
				const std::string master_id_str = std::to_string(master_id);
				relations_json[member_str]["master"] = master_id_str;
				if (!qq_name_json.count(master_id_str)) {
					common::get_group_member_name(api, group_id, master_id, name);
					qq_name_json[master_id_str] = name;
				}
			}
			if (mother_id) {
				const std::string mother_id_str = std::to_string(mother_id);
				relations_json[member_str]["mother"] = mother_id_str;
				if (!qq_name_json.count(mother_id_str)) {
					common::get_group_member_name(api, group_id, mother_id, name);
					qq_name_json[mother_id_str] = name;
				}
			}
			if (wife_id) {
				const std::string wife_id_str = std::to_string(wife_id);
				relations_json[member_str]["wife"] = wife_id_str;
				if (!qq_name_json.count(wife_id_str)) {
					common::get_group_member_name(api, group_id, wife_id, name);
					qq_name_json[wife_id_str] = name;
				}
			}
		}
		{
			std::ofstream ofs_qq_name(common::RELATION_DIR + "qq_name.json");
			ofs_qq_name << qq_name_json.dump();
			std::ofstream ofs_relations(common::RELATION_DIR + "relations.json");
			ofs_relations << relations_json.dump();
		}
		const std::string cmd = "python3 " + common::RELATION_PY + " " + common::RELATION_DIR + "qq_name.json " +
								common::RELATION_DIR + "relations.json " + file_name;
		system(cmd.c_str());
	}
	json params{};
	params["group_id"] = group_id;
	json message = json::array();
	common::add_image_message(message, "file:///" + file_name);
	params["message"] = message;
	api("send_group_msg", params);
}
void HandleMessage::handle_subscribe(const ApiFunc& api, int64_t group_id, int64_t user_id) {
	const std::string group_id_str = std::to_string(group_id);
	const std::string user_id_str = std::to_string(user_id);
	if (!common::notice_group_member_json.contains(group_id_str)) {
		common::notice_group_member_json[group_id_str] = json::array();
	}
	auto& members = common::notice_group_member_json[group_id_str];
	if (std::find(members.begin(), members.end(), user_id_str) == members.end()) {
		members.push_back(user_id_str);
		CommandRouter::save_notice_group_member_data();
	}
	json params{};
	params["group_id"] = group_id;
	json message = json::array();
	common::add_at_message(message, user_id);
	common::add_text_message(message, "\n 订阅成功喵~");
	params["message"] = message;
	api("send_group_msg", params);
}
void HandleMessage::handle_unsubscribe(const ApiFunc& api, int64_t group_id, int64_t user_id) {
	const std::string group_id_str = std::to_string(group_id);
	const std::string user_id_str = std::to_string(user_id);
	if (common::notice_group_member_json.contains(group_id_str)) {
		auto& members = common::notice_group_member_json[group_id_str];
		members.erase(std::remove(members.begin(), members.end(), user_id_str), members.end());
		CommandRouter::save_notice_group_member_data();
	}
	json params{};
	params["group_id"] = group_id;
	json message = json::array();
	common::add_at_message(message, user_id);
	common::add_text_message(message, "\n 取消订阅成功喵~");
	params["message"] = message;
	api("send_group_msg", params);
}
void HandleMessage::handle_get_sex_image(const ApiFunc& api, int64_t group_id, int64_t time) {
	std::vector<std::string> files;
	common::get_files(common::SEX_DIR, files);
	json params{};
	params["group_id"] = group_id;
	json message = json::array();
	if (files.empty()) {
		common::add_text_message(message, "没有色图了喵~");
	}
	else {
		const std::string random_file = files[time % files.size()];
		common::add_image_message(message, "file:///" + common::SEX_DIR + random_file);
	}
	params["message"] = message;
	api("send_group_msg", params);
}
void HandleMessage::filter_valid_messages(json& message_array) {
	message_array.erase(std::remove_if(message_array.begin(),
									   message_array.end(),
									   [](json& message) {
										   if (message["type"] != "text") {
											   return false;
										   }
										   auto& text = message["data"]["text"].get_ref<std::string&>();
										   common::trim(text);
										   return text.empty();
									   }),
						message_array.end());
}
bool HandleMessage::is_commond_of_upload_sex_image(const std::string& raw_message, const json& message_array) {
	if (raw_message.find("上传色图") != std::string::npos) {
		return true;
	}
	bool has_upload_command = false;
	bool has_call_yun = false;
	for (const auto& msg : message_array) {
		const auto& type = msg.at("type");
		if (type == "text") {
			const auto& text = msg.at("data").at("text").get<std::string>();
			for (const auto& keyword : common::sex_upload_commond_keywords) {
				has_upload_command |= text.find(keyword) != std::string::npos;
			}
			has_call_yun |= text.find("小云") != std::string::npos;
		}
		else if (type == "at") {
			has_call_yun |= std::stoll(msg.at("data").at("qq").get<std::string>()) == common::CONFIG_STATIC.yun_robot_qq;
		}
		if (has_upload_command && has_call_yun) {
			return true;
		}
	}
	return false;
}
void HandleMessage::handle_upload_sex_image(const ApiFunc& api,
											const json& message_array,
											int64_t group_id,
											int64_t user_id) {
	if (common::bans.count(std::to_string(user_id))) {
		handle_no_permission(api, group_id, user_id);
		return;
	}
	int suc = 0;
	int total = 0;
	json message = json::array();
	for (const auto& msg : message_array) {
		const auto& type = msg.at("type");
		if (type == "image") {
			download_sex_image(api, msg.at("data"), common::SEX_REVIEW_DIR, suc, total, message);
		}
		else if (type == "reply") {
			json params{{"message_id", msg.at("data").at("id")}};
			const auto reply_messages = api("get_msg", params).at("data").at("message");
			download_sex_images(api, reply_messages, common::SEX_REVIEW_DIR, suc, total, message);
		}
	}
	common::add_text_message(message, "上传完成！成功率：" + std::to_string(suc) + "/" + std::to_string(total));
	api("send_group_msg", {{"group_id", group_id}, {"message", message}});
}
bool HandleMessage::upload_eat_or_drink_image(const ApiFunc& api,
											  const json& message_array,
											  int64_t group_id,
											  std::string& word) {
	for (const auto& msg : message_array) {
		const auto& type = msg.at("type");
		if (type == "text") {
			const auto& text = msg.at("data").at("text").get<std::string>();
			if (common::starts_with_and_trim(text, "吃！", word)) {
				handle_upload_eat_or_drink_image(api, message_array, word, group_id, true);
				return true;
			}
			if (common::starts_with_and_trim(text, "喝！", word)) {
				handle_upload_eat_or_drink_image(api, message_array, word, group_id, false);
				return true;
			}
		}
	}
	return false;
}
void HandleMessage::handle_upload_eat_or_drink_image(
	const ApiFunc& api, const json& message_array, const std::string& word, int64_t group_id, bool is_eat) {
	const std::string save_path = (is_eat ? common::EAT_REVIEW_DIR : common::DRINK_REVIEW_DIR) + word;
	bool suc = false;
	for (const auto& msg : message_array) {
		const auto& type = msg.at("type");
		if (type == "image") {
			const auto image_data = msg.at("data");
			const std::string url = image_data.at("url").get<std::string>();
			const auto response = api("download_file", json{{"url", url}, {"name", save_path}});
			const int64_t retcode = response["retcode"].get<int64_t>();
			suc = !retcode;
			break;
		}
		else if (type == "reply") {
			const auto reply_messages =
				api("get_msg", {{"message_id", msg.at("data").at("id")}}).at("data").at("message");
			for (const auto& reply_msg : reply_messages) {
				const auto& reply_type = reply_msg.at("type");
				if (reply_type == "image") {
					const auto image_data = reply_msg.at("data");
					const std::string url = image_data.at("url").get<std::string>();
					const auto response = api("download_file", json{{"url", url}, {"name", save_path}});
					const int64_t retcode = response["retcode"].get<int64_t>();
					suc = !retcode;
					break;
				}
			}
			break;
		}
	}
	json params{};
	params["group_id"] = group_id;
	json message = json::array();
	if (is_eat) {
		if (suc) {
			common::add_text_message(message, "Rusk最喜欢吃" + word + "了！真好吃！");
		}
		else {
			common::add_text_message(message, "呸呸呸！" + word + "真难吃！");
		}
	}
	else {
		if (suc) {
			common::add_text_message(message, "吨吨吨！" + word + "真好喝！");
		}
		else {
			common::add_text_message(message, "yue~ " + word + "真难喝！");
		}
	}
	params["message"] = message;
	api("send_group_msg", params);
}
void HandleMessage::handle_ban_or_allow(
	const ApiFunc& api, const json& message_array, int64_t group_id, int64_t user_id, bool is_ban) {
	if (common::CONFIG_STATIC.admin_qq != user_id) {
		handle_no_permission(api, group_id, user_id);
		return;
	}
	json message = json::array();
	for (const auto& msg : message_array) {
		if (msg.at("type") == "at") {
			const std::string& target_id = msg.at("data").at("qq").get<std::string>();
			if (is_ban) {
				common::bans.insert(target_id);
			}
			else {
				common::bans.erase(target_id);
			}
			common::add_at_message(message, std::stoll(target_id));
		}
	}
	common::add_text_message(message, is_ban ? "已封禁" : "已解封");
	api("send_group_msg", {{"group_id", group_id}, {"message", message}});
	CommandRouter::save_ban_data();
}
void HandleMessage::download_sex_images(
	const ApiFunc& api, const json& messages, const std::string& save_dir, int& suc, int& total, json& message) {
	for (const auto& msg : messages) {
		if (msg.at("type") == "image") {
			download_sex_image(api, msg.at("data"), save_dir, suc, total, message);
		}
	}
}
void HandleMessage::download_sex_image(
	const ApiFunc& api, const json& image_data, const std::string& save_dir, int& suc, int& total, json& message) {
	++total;
	const std::string file_name = image_data.at("file").get<std::string>();
	const std::string url = image_data.at("url").get<std::string>();
	const std::string save_path = save_dir + file_name;
	const auto response = api("download_file", {{"url", url}, {"name", save_path}});
	if (response.at("retcode").get<int64_t>() == 0) {
		++suc;
	}
	else {
		common::add_text_message(message, response.at("message").get<std::string>() + "\n");
	}
}
void HandleMessage::handle_no_permission(const ApiFunc& api, int64_t group_id, int64_t user_id) {
	json params{};
	params["group_id"] = group_id;
	json message = json::array();
	common::add_at_message(message, user_id);
	common::add_text_message(message, "\n权限不足");
	params["message"] = message;
	api("send_group_msg", params);
}
/*
1.急急急
注意读写文件的线程安全问题
去掉api参数
加日志
将今日老婆等命令改为抽/换老婆，每日限制3次，增加查关系和取消功能，增加一键抽功能，断绝关系功能

2.必要
性能优化
重构
帮助菜单
指令调用统计
根据发送信息大小调整发送间隔
功能开关

3.有用
崩溃自动重启
优化传旨功能
今日说法/吃瓜省流
群分析
用户画像
群成员分布情况（性别、地域、等级等）
视频转发
设置群提醒
设置群成员专属提醒
日历
上传文件自动压缩
哔哩哔哩直播监控

4.有趣的功能
老婆跑了等
个人关系图提取
可不可以
戳一戳哈气
大富翁
发送表情包
今日龙王榜
传旨

5.vrc相关功能
vrc id 绑定
vrc api调用
*/