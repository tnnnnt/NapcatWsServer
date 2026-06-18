#include "handle_message.h"
#include <fstream>
#include <string>
#include <vector>
#include "../command_router.h"
#include "../common.hpp"

void HandleMessage::start(const json& event, std::function<json(const std::string&, const json&)> api) {
	const std::string message_type = event.at("message_type").get<std::string>();
	if (message_type == "private") {
	}
	else if (message_type == "group") {
		const int64_t group_id = event.at("group_id").get<int64_t>();
		if (group_id == common::TEST_GROUP) {
			json params{};
			params["group_id"] = group_id;
			json message = json::array();
			common::add_text_message(message, event.dump(4));
			params["message"] = message;
			api("send_group_msg", params);
		}
		const int64_t user_id = event.at("user_id").get<int64_t>();
		{
			std::lock_guard<std::mutex> lock(common::today_group_member_message_number_mutex);
			++common::today_group_member_message_number[group_id][user_id];
		}
		const auto time = event.at("time").get<int64_t>() + common::TIME_ZONE_OFFSET; // 转为北京时间
		const std::string raw_message = event.at("raw_message").get<std::string>();

		if (raw_message.find("吃什么") != std::string::npos || raw_message.find("吃啥") != std::string::npos ||
			raw_message.find("喝什么") != std::string::npos || raw_message.find("喝啥") != std::string::npos) {
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
			return;
		}

		const int64_t seed = time / 86400 + user_id; // 每天每人一个固定的 seed
		int luckey_num = 0;							 // 初始化，避免未定义行为
		json message_array = event.at("message");
		filter_valid_messages(message_array);
		const auto message_size = message_array.size();
		if (message_size == 1) {
			const auto& seg_obj = message_array[0];
			const std::string type = seg_obj.at("type").get<std::string>();
			if (type == "text") {
				const std::string text = seg_obj.at("data").at("text").get<std::string>();
				std::string word;
				if (text == "今日运势") {
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
					common::add_text_message(message,
											 "\n今日运势（仅供娱乐）\n宜：" + fortunes[0] + " " + fortunes[1] + " " +
												 fortunes[2] + "\n忌：" + fortunes[3] + " " + fortunes[4] + " " +
												 fortunes[5] + "\n幸运数字：" + std::to_string(luckey_num));
					params["message"] = message;
					api("send_group_msg", params);
				}
				else if (text == "今日老婆" || text == "今日老公" || text == "今日妈妈" || text == "今日主人") {
					std::vector<int64_t> temp;
					{
						std::lock_guard<std::mutex> lock(common::group_members_mutex);
						temp =
							(common::group_active_members[group_id].empty() ? common::group_members
																			: common::group_active_members)[group_id];
					}
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
					common::add_image_message(message,
											  "https://q.qlogo.cn/g?b=qq&nk=" + std::to_string(match_id) + "&s=4");
					params["message"] = message;
					api("send_group_msg", params);
				}
				else if (text == "关系图") {
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
						const std::string cmd = "python3 " + common::SCRIPT_DIR + "relations.py " +
												common::RELATION_DIR + "qq_name.json " + common::RELATION_DIR +
												"relations.json " + file_name;
						system(cmd.c_str());
					}
					json params{};
					params["group_id"] = group_id;
					json message = json::array();
					common::add_image_message(message, "file:///" + file_name);
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
						common::add_image_message(message, "file:///" + common::SEX_DIR + random_file);
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
						std::vector<std::pair<int64_t, int>> member_message_rank;
						{
							std::lock_guard<std::mutex> lock(common::today_group_member_message_number_mutex);
							const auto& member_message_number = common::today_group_member_message_number[group_id];
							member_message_rank.insert(
								member_message_rank.end(), member_message_number.begin(), member_message_number.end());
						}
						const size_t k = std::min(common::RANK_SIZE, member_message_rank.size());
						std::partial_sort(member_message_rank.begin(),
										  member_message_rank.begin() + k,
										  member_message_rank.end(),
										  [](const std::pair<int64_t, int>& a, const std::pair<int64_t, int>& b) {
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
			}
			else if (type == "at") {
				const int64_t qq = std::stoll(seg_obj.at("data").at("qq").get<std::string>());
				if (qq == common::ROBOT_QQ) {
					json params{};
					params["group_id"] = group_id;
					json message = json::array();
					common::add_text_message(message, "干什么！");
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
			if (type0 == "reply") {
				const std::string message_id = seg_obj0.at("data").at("id").get<std::string>();
				if (type1 == "text") {
					const std::string text = seg_obj1.at("data").at("text").get<std::string>();
					std::string word;
					if (text == "/上传色图") {
						if (common::is_ban(user_id)) {
							json params{};
							params["group_id"] = group_id;
							json message = json::array();
							common::add_at_message(message, user_id);
							common::add_text_message(message, "\n权限不足");
							params["message"] = message;
							api("send_group_msg", params);
						}
						else {
							json params{};
							params["message_id"] = message_id;
							const auto messages_json = api("get_msg", params)["data"].at("message");
							int suc = 0;
							int total = 0;
							json message = json::array();
							for (const auto& message_json : messages_json) {
								if (message_json.at("type") == "image") {
									++total;
									const auto image_data = message_json.at("data");
									const std::string file_name = image_data.at("file").get<std::string>();
									const std::string url = image_data.at("url").get<std::string>();
									const std::string save_path = "../../../../../" + common::SEX_DIR + file_name;
									const auto response = api("download_file", json{{"url", url}, {"name", save_path}});
									const int64_t retcode = response["retcode"].get<int64_t>();
									if (retcode == 0) {
										++suc;
									}
									else {
										common::add_text_message(message,
																 response["message"].get<std::string>() + "\n");
									}
								}
							}
							params["group_id"] = group_id;
							common::add_text_message(
								message, "上传完成！成功率：" + std::to_string(suc) + "/" + std::to_string(total));
							params["message"] = message;
							api("send_group_msg", params);
						}
					}
					else if (common::starts_with_and_trim(text, "/吃！", word)) {
						if (common::is_ban(user_id)) {
							json params{};
							params["group_id"] = group_id;
							json message = json::array();
							common::add_at_message(message, user_id);
							common::add_text_message(message, "\n权限不足");
							params["message"] = message;
							api("send_group_msg", params);
						}
						else {
							json params{};
							params["message_id"] = message_id;
							const auto messages_json = api("get_msg", params)["data"].at("message");
							bool suc = false;
							for (const auto& message_json : messages_json) {
								if (message_json.at("type") == "image") {
									const auto image_data = message_json.at("data");
									const std::string url = image_data.at("url").get<std::string>();
									const std::string save_path = "../../../../../" + common::EAT_DIR + word;
									const auto response = api("download_file", json{{"url", url}, {"name", save_path}});
									const int64_t retcode = response["retcode"].get<int64_t>();
									if (retcode == 0) {
										suc = true;
										break;
									}
								}
							}
							params["group_id"] = group_id;
							json message = json::array();
							if (suc) {
								common::add_text_message(message, "Rusk最喜欢吃" + word + "了！真好吃！");
							}
							else {
								common::add_text_message(message, "呸呸呸！" + word + "真难吃！");
							}
							params["message"] = message;
							api("send_group_msg", params);
						}
					}
					else if (common::starts_with_and_trim(text, "/喝！", word)) {
						if (common::is_ban(user_id)) {
							json params{};
							params["group_id"] = group_id;
							json message = json::array();
							common::add_at_message(message, user_id);
							common::add_text_message(message, "\n权限不足");
							params["message"] = message;
							api("send_group_msg", params);
						}
						else {
							json params{};
							params["message_id"] = message_id;
							const auto messages_json = api("get_msg", params)["data"].at("message");
							bool suc = false;
							for (const auto& message_json : messages_json) {
								if (message_json.at("type") == "image") {
									const auto image_data = message_json.at("data");
									const std::string url = image_data.at("url").get<std::string>();
									const std::string save_path = "../../../../../" + common::DRINK_DIR + word;
									const auto response = api("download_file", json{{"url", url}, {"name", save_path}});
									const int64_t retcode = response["retcode"].get<int64_t>();
									if (retcode == 0) {
										suc = true;
										break;
									}
								}
							}
							params["group_id"] = group_id;
							json message = json::array();
							if (suc) {
								common::add_text_message(message, "吨吨吨！" + word + "真好喝！");
							}
							else {
								common::add_text_message(message, "yue~ " + word + "真难喝！");
							}
							params["message"] = message;
							api("send_group_msg", params);
						}
					}
				}
			}
			else if (type0 == "text") {
				const std::string text = seg_obj0.at("data").at("text").get<std::string>();
				if (text == "/ban") {
					if (common::ADMIN_QQ == user_id) {
						if (type1 == "at") {
							const int64_t ban_user_id = std::stoll(seg_obj1.at("data").at("qq").get<std::string>());
							std::ofstream ofs(common::BAN_FILE, std::ios::app);
							ofs << ban_user_id << "\n";
							ofs.close();
							json params{};
							params["group_id"] = group_id;
							json message = json::array();
							common::add_at_message(message, ban_user_id);
							common::add_text_message(message, "已封禁");
							params["message"] = message;
							api("send_group_msg", params);
						}
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
				else if (text == "/allow") {
					if (common::ADMIN_QQ == user_id) {
						if (type1 == "at") {
							const int64_t allow_user_id = std::stoll(seg_obj1.at("data").at("qq").get<std::string>());
							std::vector<int64_t> bans;
							std::ifstream ifs(common::BAN_FILE);
							int64_t ban;
							while (ifs >> ban) {
								bans.push_back(ban);
							}
							ifs.close();
							bans.erase(std::remove(bans.begin(), bans.end(), allow_user_id), bans.end());
							std::ofstream ofs(common::BAN_FILE);
							for (const int64_t& ban : bans) {
								ofs << ban << "\n";
							}
							ofs.close();
							json params{};
							params["group_id"] = group_id;
							json message = json::array();
							common::add_at_message(message, allow_user_id);
							common::add_text_message(message, "已解封");
							params["message"] = message;
							api("send_group_msg", params);
						}
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
			}
		}
	}
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
/*
重构
上传文件审核区
云姐帮忙上传
色图整理（手动）
色图投票/色图打分系统，根据分数给抽中概率加权重
快速删除文件
bot被移出群聊闪退bug
可不可以
将今日老婆等命令改为抽/换老婆，每日限制3次，增加查关系功能，增加一键抽功能
优化生成关系图性能
个人关系图提取
优化传旨功能
加日志
性能优化
戳一戳哈气
今日说法
求婚
崩溃自动重启
吃瓜省流*
群分析*
用户画像*
大富翁
vrc id 绑定
群成员分布情况（性别、地域、等级等）
视频转发
指令调用统计
设置群提醒
设置群成员专属提醒
充值系统
日历
帮助菜单
统计（调用次数）
vrc api调用
*/