#include "../command_router.h"
#include "../common.hpp"
#include "handle_message.h"
#include <fstream>
#include <string>
#include <vector>

void HandleMessage::start(const json& event, std::function<json(const std::string&, const json&)> api) {
	const std::string message_type = event.at("message_type").get<std::string>();
	if (message_type == "private") {
		json params{};
		params["user_id"] = event.at("user_id");
		json message = json::array();
		common::add_text_message(message, "暂时不支持私聊，请催促tnt更新");
		params["message"] = message;
		api("send_private_msg", params);
	}
	else if (message_type == "group") {
		const auto time = event.at("time").get<int64_t>() + common::TIME_ZONE_OFFSET; // 转为北京时间
		const int64_t group_id = event.at("group_id").get<int64_t>();
		const int64_t user_id = event.at("user_id").get<int64_t>();
		if (group_id == common::TEST_GROUP) {
			json params{};
			params["group_id"] = group_id;
			json message = json::array();
			common::add_text_message(message, event.dump(4));
			params["message"] = message;
			api("send_group_msg", params);
		}
		{
			std::lock_guard<std::mutex> lock(common::today_group_member_message_number_mutex);
			++common::today_group_member_message_number[group_id][user_id];
		}
		const auto& message_array = event.at("message");
		const int64_t seed = time / 86400 + user_id; // 每天每人一个固定的 seed
		const auto message_size = message_array.size();
		int luckey_num = 0; // 初始化，避免未定义行为
		if (message_size == 1) {
			const auto& seg_obj = message_array[0];
			const std::string type = seg_obj.at("type").get<std::string>();
			if (type == "text") {
				const std::string text = seg_obj.at("data").at("text").get<std::string>();
				if (text == "今日运势")
				{
					json params{};
					params["group_id"] = group_id;
					json message = json::array();
					std::vector<std::string> fortunes; // 运势
					std::string fortune;
					std::ifstream ifs(common::FORTUNE_FILE);
					while (std::getline(ifs, fortune)) {
						fortunes.push_back(fortune);
					}
					ifs.close();
					common::shuffle_vector(fortunes, luckey_num, seed);
					common::add_at_message(message, user_id);
					common::add_text_message(message, "\n今日运势（仅供娱乐）\n宜：" + fortunes[0] + " " + fortunes[1] + " " + fortunes[2] +
						"\n忌：" + fortunes[3] + " " + fortunes[4] + " " + fortunes[5] + "\n幸运数字：" + std::to_string(luckey_num));
					params["message"] = message;
					api("send_group_msg", params);
				}
				else if (text == "今日老婆" || text == "今日老公" || text == "今日妈妈" || text == "今日主人")
				{
					std::vector<int64_t>temp;
					{
						std::lock_guard<std::mutex> lock(common::group_members_mutex);
						temp = (common::group_active_members[group_id].empty() ? common::group_members : common::group_active_members)[group_id];
					}
					common::shuffle_vector(temp, luckey_num, seed);
					const int member_count = temp.size();
					int64_t match_id;
					if (text == "今日老婆")match_id = temp[0];
					else if (text == "今日老公")match_id = temp[std::min(member_count - 1, 1)];
					else if (text == "今日妈妈")match_id = temp[std::min(member_count - 1, 2)];
					else if (text == "今日主人")match_id = temp[std::min(member_count - 1, 3)];

					const std::string match_name = api("get_group_member_info", json{ {"group_id", std::to_string(group_id)}, { "user_id", std::to_string(match_id) } })["data"].at("nickname").get<std::string>();
					json params{};
					params["group_id"] = group_id;
					json message = json::array();
					common::add_at_message(message, user_id);
					std::string last_sentence;
					if (text == "今日老婆") last_sentence = "请好好对待她哦~";
					else if (text == "今日老公") last_sentence = "请好好对待他哦~";
					else if (text == "今日妈妈") last_sentence = "快去叫妈妈！";
					else if (text == "今日主人") last_sentence = "快去叫主人！";
					common::add_text_message(message, "\n你的" + text + "是【" + match_name + "】\n" + last_sentence);
					common::add_image_message(message, "https://q.qlogo.cn/g?b=qq&nk=" + std::to_string(match_id) + "&s=4");
					params["message"] = message;
					api("send_group_msg", params);
				}
				else if (text == "今日龙王榜") {
					CommandRouter::send_today_group_member_message_number_data(group_id, api);
				}
				else if (text == "订阅通知") {
					CommandRouter::add_notice_group_member(group_id, user_id);
					json params{};
					params["group_id"] = group_id;
					json message = json::array();
					common::add_at_message(message, user_id);
					common::add_text_message(message, "\n 订阅成功喵~");
					params["message"] = message;
					api("send_group_msg", params);
				}
				else if (text == "取消订阅") {
					CommandRouter::del_notice_group_member(group_id, user_id);
					json params{};
					params["group_id"] = group_id;
					json message = json::array();
					common::add_at_message(message, user_id);
					common::add_text_message(message, "\n 取消订阅成功喵~");
					params["message"] = message;
					api("send_group_msg", params);
				}
				else if (text == "来点色图") {
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
						common::add_image_message(message, "file:///" + common::SEX_DIR + "/" + random_file);
					}
					params["message"] = message;
					api("send_group_msg", params);
				}
				else if (text[0] == '/') {
					if (common::ADMIN_QQ == user_id) {
						const std::string command = text.substr(1);
						json params{};
						params["group_id"] = group_id;
						json message = json::array();
						common::add_text_message(message, "传");
						common::add_at_message(message, user_id);
						common::add_text_message(message, "口谕：\n\n");
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
							common::add_at_message(message, user_id);
						}
						common::add_text_message(message, "\n\n" + command);
						params["message"] = message;
						api("send_group_msg", params);
					}
					else {
						json params{};
						params["group_id"] = group_id;
						json message = json::array();
						common::add_at_message(message, user_id);
						common::add_text_message(message, "\n权限不足");
						params["message"] = message;
						api("send_group_msg", params);
					}
				}
				else if (text.find("吃什么") != std::string::npos || text.find("吃啥") != std::string::npos || text.find("喝什么") != std::string::npos || text.find("喝啥") != std::string::npos) {
					const bool is_eat = text.find("吃") != std::string::npos;
					const auto& dir = is_eat ? common::EAT_DIR : common::DRINK_DIR;
					std::vector<std::string> files;
					common::get_files(dir, files);
					json params{};
					params["group_id"] = group_id;
					json message = json::array();
					if (files.empty()) {
						common::add_text_message(message, is_eat ? "别吃" : "别喝");
					}
					else {
						const std::string random_file = files[time % files.size()];
						common::add_text_message(message, "推荐" + common::remove_extension(random_file));
						common::add_image_message(message, "file:///" + dir + "/" + random_file);
					}
					params["message"] = message;
					api("send_group_msg", params);
				}
			}
		}
		else if (message_size == 2) {
			const auto& seg_obj0 = message_array[0];
			const std::string type0 = seg_obj0.at("type").get<std::string>();
			const auto& seg_obj1 = message_array[1];
			const std::string type1 = seg_obj1.at("type").get<std::string>();
			if (type0 == "at") {
				const int64_t qq = std::stoll(seg_obj0.at("data").at("qq").get<std::string>());
				if (qq == common::ROBOT_QQ) {
					if (type1 == "text") {
						const std::string text = seg_obj1.at("data").at("text").get<std::string>();
						if (common::is_only_spaces(text)) {
							json params{};
							params["group_id"] = group_id;
							json message = json::array();
							common::add_text_message(message, "干什么！");
							params["message"] = message;
							api("send_group_msg", params);
						}
					}
				}
			}
		}
	}
}
/*
加日志功能
上传文件功能(仅tnt)
使用群昵称
求婚
关系图
崩溃自动重启
简单对话*
吃瓜省流*
群分析*
用户画像*
1. 近期群成员变动情况
3. 大富翁
4. vrc id 绑定
6. 群成员分布情况（性别、地域、等级等）
7. 视频转发
8. 指令调用统计
9. 设置群提醒
10. 设置群成员专属提醒
11. 充值系统
12. 日历
13. 帮助菜单
19. 统计（调用次数）
20. vrc api调用
*/