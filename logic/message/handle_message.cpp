#include "handle_message.h"
#include <cstdlib>
#include <vector>
#include <string>
#include <filesystem>

const static int64_t ROBOT_QQ = 3777014797; // 替换为你的机器人 QQ 号
const static std::string WORK_DIR = "/home/bot/qq_robot"; // 替换为你的工作目录路径
const static std::string EAT_DIR = WORK_DIR + "/eat"; // 吃什么
// 使用指定 seed 打乱 vector 顺序（原地修改）
template<typename T>
static void shuffle_vector(std::vector<T>& v, int& luckey_num, const int64_t& seed)
{
	std::mt19937 rng(seed);
	luckey_num = rng() % 100;
	std::shuffle(v.begin(), v.end(), rng);
}

static void get_files(const fs::path& dir_path, std::vector<std::string>& files) {
	files.clear();
	if (!fs::exists(dir_path) || !fs::is_directory(dir_path)) {
		throw std::runtime_error("Path is not a valid directory.");
	}
	for (const auto& entry : fs::directory_iterator(dir_path)) {
		if (entry.is_regular_file()) {
			files.push_back(entry.path().filename());
		}
	}
}

static std::string remove_extension(const std::string& filename) {
	const std::size_t pos = filename.find_last_of('.');
	if (pos == std::string::npos)
		return filename;  // 没有后缀
	return filename.substr(0, pos);
}

// 判断字符串是否仅包含空格
static bool is_only_spaces(const std::string& s)
{
	return !s.empty() && std::all_of(s.begin(), s.end(), [](char c) { return c == ' '; });
}

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
		const auto group_id = event.at("group_id").get<int64_t>();
		const auto user_id = event.at("user_id").get<int64_t>();
		const auto& message_array = event.at("message");
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
						"旅行", "结婚", "生孩子", "搬家", "买东西", "看电影", "看书", "水群", "吃瓜", "遛狗", "遛猫", "钓鱼",
						"打台球", "打麻将", "KTV" }; // 运势
					const int64_t seed = time / 86400 + user_id; // 每天每人一个固定的 seed
					int luckey_num;
					shuffle_vector(fortune, luckey_num, seed);
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
				else if (text.find("吃什么") != std::string::npos) {
					std::vector<std::string> files;
					get_files(EAT_DIR, files);
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
								{"text", "推荐" + remove_extension(random_file)}
							}}
							});
						message.emplace_back(json{
							{"type", "image"},
							{"data", json{
								{"file", "file:///" + EAT_DIR + "/" + random_file}
							}}
							});
					}
					params["message"] = message;
					api("send_group_msg", params);
				}
			}
			else if (type == "at") {
				const int64_t qq = std::stoll(seg_obj.at("data").at("qq").get<std::string>());
				if (qq == ROBOT_QQ) {
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
				if (qq == ROBOT_QQ) {
					if (type1 == "text") {
						const std::string text = seg_obj1.at("data").at("text").get<std::string>();
						if (is_only_spaces(text)) {
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