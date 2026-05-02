#pragma once
#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <mutex>
#include <nlohmann/json.hpp>
#include <random>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>
#include <fstream>

namespace fs = std::filesystem;

namespace common
{
	inline const std::string WORK_DIR = "/home/bot/qq_robot/"; // 替换为你的工作目录路径
	inline const std::string CONFIG_FILE = WORK_DIR + "config.json"; // 配置文件
	inline const std::string SAVE_DIR = WORK_DIR + "save/"; // 数据保存目录
	inline const std::string EAT_DIR = common::WORK_DIR + "eat"; // 吃什么
	inline const std::string DRINK_DIR = common::WORK_DIR + "drink"; // 喝什么
	inline const std::string SEX_DIR = common::WORK_DIR + "sex"; // 色图
	inline const std::string FORTUNE_FILE = common::SAVE_DIR + "fortunes.txt";
	inline const std::string BAN_FILE = common::SAVE_DIR + "ban.txt";
	inline const std::string NOTICE_GROUP_MEMBER_FILE = common::SAVE_DIR + "notice_group_member.json";
	inline const std::string TODAY_GROUP_MEMBER_MESSAGE_NUMBER_FILE = common::SAVE_DIR + "today_group_member_message_number.json"; // 今日群成员发言数文件
	inline const int64_t TEST_GROUP = 1092859942; // 测试群
	
	inline int64_t ADMIN_QQ; // 管理员 QQ 号
	inline int64_t ROBOT_QQ; // 机器人 QQ 号
	inline size_t POOL_SIZE; // 线程池大小
	inline size_t RANK_SIZE; // 排行榜大小
	inline size_t BASE_DELAY; // 基础延迟（秒）
	inline size_t RANDOM_DELAY; // 随机延迟（秒）
	inline size_t TIME_SAVE_INTERVAL; // 数据保存时间间隔（秒）
	inline size_t TIME_ZONE_OFFSET; // 时区偏移（秒）
	inline size_t MIN_ACTIVITY_LEVEL; // 最小活跃度要求（群等级）
	
	inline std::mutex group_members_mutex; // 保护 group_members 的互斥锁
	inline std::vector<int64_t> group_ids; // 群列表
	inline std::unordered_map<int64_t, std::vector<int64_t>>group_members; // 群成员列表
	inline std::unordered_map<int64_t, std::vector<int64_t>>group_active_members; // 活跃群成员列表

	inline std::mutex today_group_member_message_number_mutex; // 保护 group_member_message_number 的互斥锁
	inline std::unordered_map<int64_t, std::unordered_map<int64_t, int>>today_group_member_message_number; // 今日群成员发言数
	
	// 判断字符串是否仅包含空格
	inline bool is_only_spaces(const std::string& s)
	{
		return !s.empty() && std::all_of(s.begin(), s.end(), [](char c) { return c == ' '; });
	}
	// 判断字符串s是否由ss开头
	inline bool starts_with_and_trim(const std::string& s, const std::string& ss, std::string& out)
	{
		if (s.size() < ss.size())
			return false;
		if (s.compare(0, ss.size(), ss) == 0)
		{
			out = s.substr(ss.size());
			return true;
		}
		return false;
	}
	// 去掉后缀
	inline std::string remove_extension(const std::string& filename) {
		const std::size_t pos = filename.find_last_of('.');
		if (pos == std::string::npos)
			return filename;
		return filename.substr(0, pos);
	}
	// 获取目录中所有文件
	inline void get_files(const fs::path& dir_path, std::vector<std::string>& files) {
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
	// 使用指定 seed 打乱 vector 顺序（原地修改）
	template<typename T>
	inline void shuffle_vector(std::vector<T>& v, int& luckey_num, const int64_t& seed)
	{
		std::mt19937 rng(seed);
		luckey_num = rng() % 100;
		std::shuffle(v.begin(), v.end(), rng);
	}
	// 判断用户是否在黑名单中
	inline bool is_ban(const int64_t& user_id) {
		std::string ban;
		std::ifstream ifs(BAN_FILE);
		while (std::getline(ifs, ban)) {
			if (std::stoll(ban) == user_id) {
				ifs.close();
				return true;
			}
		}
		ifs.close();
		return false;
	}
	//纯文本
	inline void add_text_message(json& message, const std::string& text) {
		message.emplace_back(json{ {"type", "text"},{"data", json{{"text", text}}} });
	}
	//图片
	inline void add_image_message(json& message, const std::string& file_path) {
		message.emplace_back(json{ {"type", "image"},{"data", json{{"file", file_path}}} });
	}
	//@某人
	inline void add_at_message(json& message, const int64_t& qq) {
		message.emplace_back(json{ {"type", "at"},{"data", json{{"qq", qq}}} });
	}
	//回复
	inline void add_reply_message(json& message, const int& message_id) {
		message.emplace_back(json{ {"type", "reply"},{"data", json{{"id", message_id}}} });
	}
}