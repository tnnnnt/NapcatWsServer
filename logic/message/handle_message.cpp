#include "../common.hpp"
#include "handle_message.h"
#include <string>
#include <vector>

void HandleMessage::start(const json& event, std::function<json(const std::string&, const json&)> api) {
	const std::string message_type = event.at("message_type").get<std::string>();
	if (message_type == "private") {
		json params{};
		params["user_id"] = event.at("user_id");
		params["message"] = "暂时不支持私聊，请催促tnt更新";
		api("send_private_msg", params);
	}
	else if (message_type == "group") {
		const auto time = event.at("time").get<int64_t>() + 28800; // 转为北京时间
		const int64_t group_id = event.at("group_id").get<int64_t>();
		const auto user_id = event.at("user_id").get<int64_t>();
		const auto& message_array = event.at("message");
		const int64_t seed = time / 86400 + user_id; // 每天每人一个固定的 seed
		const auto message_size = message_array.size();
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
					std::vector<std::string> fortune = { "出行", "交友", "恋爱", "相亲", "工作", "面试", "VRChat", "游戏",
						"学习", "睡觉", "吃饭", "喝水", "运动", "理财", "打扫卫生", "写代码", "unity", "摸鱼", "买彩票", "请客",
						"旅行", "结婚", "生孩子", "搬家", "购物", "看电影", "看书", "水群", "吃瓜", "遛狗", "遛猫", "钓鱼",
						"打台球", "打麻将", "KTV", "跳舞", "blender", "打篮球", "羽毛球", "面基", "骑行", "游泳", "加班"}; // 运势
					int luckey_num;
					common::shuffle_vector(fortune, luckey_num, seed);
					message.emplace_back(json{
						{"type", "at"},
						{"data", json{
							{"qq", user_id}
						}}
						});
					message.emplace_back(json{
						{"type", "text"},
						{"data", json{
							{"text", "\n今日运势（仅供娱乐）\n宜：" + fortune[0] + " " + fortune[1] + " " + fortune[2] +
							"\n忌：" + fortune[3] + " " + fortune[4] + " " + fortune[5] + "\n幸运数字：" + std::to_string(luckey_num)}
						}}
						});
					params["message"] = message;
					api("send_group_msg", params);
				}
				else if (text == "今日老婆")
				{
					std::lock_guard<std::mutex> lock(common::group_members_mutex);
					const auto group_member_num = common::group_members[group_id].size();
					const int64_t wife_id = common::group_members[group_id][seed % group_member_num];
					const std::string wife_name = api("get_group_member_info", json{ {"group_id", std::to_string(group_id)}, { "user_id", std::to_string(wife_id) } })["data"].at("nickname").get<std::string>();
					json params{};
					params["group_id"] = group_id;
					json message = json::array();
					message.emplace_back(json{
						{"type", "at"},
						{"data", json{
							{"qq", user_id}
						}}
						});
					message.emplace_back(json{
						{"type", "text"},
						{"data", json{
							{"text", "\n你的今日老婆是【" + wife_name + "】\n请好好对待她哦~"}
						}}
						});
					message.emplace_back(json{
						{"type", "image"},
						{"data", json{
							{"file", "https://q.qlogo.cn/g?b=qq&nk=" + std::to_string(wife_id) + "&s=160"}
						}}
						});
					params["message"] = message;
					api("send_group_msg", params);
				}
				else if (text == "今日老公")
				{
					std::lock_guard<std::mutex> lock(common::group_members_mutex);
					const auto group_member_num = common::group_members[group_id].size();
					const int64_t wife_id = common::group_members[group_id][(seed * seed) % group_member_num];
					const std::string wife_name = api("get_group_member_info", json{ {"group_id", std::to_string(group_id)}, { "user_id", std::to_string(wife_id) } })["data"].at("nickname").get<std::string>();
					json params{};
					params["group_id"] = group_id;
					json message = json::array();
					message.emplace_back(json{
						{"type", "at"},
						{"data", json{
							{"qq", user_id}
						}}
						});
					message.emplace_back(json{
						{"type", "text"},
						{"data", json{
							{"text", "\n你的今日老公是【" + wife_name + "】\n请好好对待他哦~"}
						}}
						});
					message.emplace_back(json{
						{"type", "image"},
						{"data", json{
							{"file", "https://q.qlogo.cn/g?b=qq&nk=" + std::to_string(wife_id) + "&s=160"}
						}}
						});
					params["message"] = message;
					api("send_group_msg", params);
				}
				else if (text.find("吃什么") != std::string::npos || text.find("吃啥") != std::string::npos) {
					std::vector<std::string> files;
					common::get_files(common::EAT_DIR, files);
					json params{};
					params["group_id"] = group_id;
					json message = json::array();
					if (files.empty()) {
						message.emplace_back(json{
							{"type", "text"},
							{"data", json{
								{"text", "别吃"}
							}}
							});
					}
					else {
						const std::string random_file = files[time % files.size()];
						message.emplace_back(json{
							{"type", "text"},
							{"data", json{
								{"text", "推荐" + common::remove_extension(random_file)}
							}}
							});
						message.emplace_back(json{
							{"type", "image"},
							{"data", json{
								{"file", "file:///" + common::EAT_DIR + "/" + random_file}
							}}
							});
					}
					params["message"] = message;
					api("send_group_msg", params);
				}
			}
			else if (type == "at") {
				const int64_t qq = std::stoll(seg_obj.at("data").at("qq").get<std::string>());
				if (qq == common::ROBOT_QQ) {
					json params{};
					params["group_id"] = group_id;
					json message = json::array();
					message.emplace_back(json{
						{"type", "text"},
						{"data", json{
							{"text", "干什么！"}
						}}
						});
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
							message.emplace_back(json{
								{"type", "text"},
								{"data", json{
									{"text", "干什么！"}
								}}
								});
							params["message"] = message;
							api("send_group_msg", params);
						}
					}
				}
			}
		}
	}
}