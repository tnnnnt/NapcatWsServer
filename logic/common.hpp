#pragma once
#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <mutex>
#include <random>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "logic_types.hpp"
namespace common {
	inline const std::string WORK_DIR = "/home/bot/qq_robot/";	// 工作目录
	inline const std::string SAVE_DIR = WORK_DIR + "save/";		// 数据保存目录
	inline const std::string SCRIPT_DIR = WORK_DIR + "script/"; // 脚本

	// 全局配置文件，仅在启动时读取一次，之后不会再读取
	inline const std::string CONFIG_STATIC_FILE = WORK_DIR + "config_static.json";
	inline int64_t ADMIN_QQ;	 // 管理员 QQ 号
	inline int64_t ROBOT_QQ;	 // 机器人 QQ 号
	inline int64_t YUN_ROBOT_QQ; // 小云机器人 QQ 号
	inline int64_t TEST_GROUP;	 // 测试群
	inline size_t POOL_SIZE;	 // 线程池大小
	inline size_t RANK_SIZE;	 // 排行榜大小

	// 周期配置文件，在启动时读取一次，之后按设置时间间隔读取
	inline const std::string CONFIG_PERIODIC_FILE = WORK_DIR + "config_periodic.json";
	inline size_t BASE_DELAY;		  // 基础延迟（秒）
	inline size_t RANDOM_DELAY;		  // 随机延迟（秒）
	inline size_t TIME_SAVE_INTERVAL; // 数据保存时间间隔（秒）

	// 每日配置文件，在启动时读取一次，之后每天0点读取一次
	inline const std::string CONFIG_DAILY_FILE = WORK_DIR + "config_daily.json";
	inline size_t MIN_ACTIVITY_LEVEL; // 最小活跃度要求（群等级）

	// 吃吃喝喝色色
	inline const std::string PATH = "../../../../../";							   // 相对路径
	inline const std::string EAT_DIR = WORK_DIR + "eat/";						   // 吃什么
	inline const std::string EAT_REVIEW_DIR = PATH + WORK_DIR + "eat_review/";	   // 吃什么审核
	inline const std::string DRINK_DIR = WORK_DIR + "drink/";					   // 喝什么
	inline const std::string DRINK_REVIEW_DIR = PATH + WORK_DIR + "drink_review/"; // 喝什么审核
	inline const std::string SEX_DIR = WORK_DIR + "sex/";						   // 色图
	inline const std::string SEX_REVIEW_DIR = PATH + WORK_DIR + "sex_review/";	   // 色图审核

	// 运势
	inline const std::string FORTUNE_FILE = SAVE_DIR + "fortunes.txt"; // 运势文件
	inline std::vector<std::string> fortunes;						   // 运势

	// 关系
	inline const std::string RELATION_PY = SCRIPT_DIR + "relations.py"; // 群成员关系文件
	inline const std::string AVATAR_DIR = WORK_DIR + "avatar/";			// 头像
	inline const std::string RELATION_DIR = WORK_DIR + "relation/";		// 关系图
	struct Relation {
		int64_t wife_id = 0;
		int64_t husband_id = 0;
		int64_t mother_id = 0;
		int64_t master_id = 0;
	}; // 群成员关系结构体
	inline std::mutex group_member_relations_mutex; // 保护 group_member_relations 的互斥锁
	inline std::unordered_map<int64_t, std::unordered_map<int64_t, Relation>>
		group_member_relations; // 群成员关系，外层 key 是 group_id，内层 key 是 user_id

	// 上传色图
	inline const std::string SEX_UPLOAD_COMMOND_KEYWORD_FILE = SAVE_DIR + "sex_upload_commond_keyword.txt"; // 关键字
	inline std::vector<std::string> sex_upload_commond_keywords; // 上传色图命令关键字列表

	// ban
	inline const std::string BAN_FILE = SAVE_DIR + "ban.txt"; // 封禁用户文件
	inline std::unordered_set<std::string> bans;			  // 封禁用户列表

	// 通知
	inline const std::string NOTICE_GROUP_MEMBER_FILE = SAVE_DIR + "notice_group_member.json"; // 群公告成员文件

	// 今日群成员发言数文件
	inline const std::string TODAY_GROUP_MEMBER_MESSAGE_NUMBER_FILE =
		SAVE_DIR + "today_group_member_message_number.json";
	inline std::mutex today_group_member_message_number_mutex; // 保护 group_member_message_number 的互斥锁
	inline std::unordered_map<int64_t, std::unordered_map<int64_t, int>>
		today_group_member_message_number; // 今日群成员发言数

	// 群基础数据
	inline std::mutex group_members_mutex;										   // 保护 group_members 的互斥锁
	inline std::vector<int64_t> group_ids;										   // 群列表
	inline std::unordered_map<int64_t, std::vector<int64_t>> group_members;		   // 群成员列表
	inline std::unordered_map<int64_t, std::vector<int64_t>> group_active_members; // 活跃群成员列表

	// 去掉字符串首尾的空白字符
	inline void trim(std::string& str) {
		const auto first = str.find_first_not_of(" \t\n\r\f\v");
		if (first == std::string::npos) {
			str.clear();
			return;
		}
		const auto last = str.find_last_not_of(" \t\n\r\f\v");
		str.erase(last + 1);
		str.erase(0, first);
	}
	// 判断字符串s是否由ss开头
	inline bool starts_with_and_trim(const std::string& s, const std::string& ss, std::string& out) {
		if (s.size() < ss.size()) {
			return false;
		}
		if (s.compare(0, ss.size(), ss) == 0) {
			out = s.substr(ss.size());
			return true;
		}
		return false;
	}
	// 去掉后缀
	inline std::string remove_extension(const std::string& filename) {
		const std::size_t pos = filename.find_last_of('.');
		if (pos == std::string::npos) {
			return filename;
		}
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
	template <typename T> inline void shuffle_vector(std::vector<T>& v, int& luckey_num, int64_t seed) {
		std::mt19937 rng(seed);
		luckey_num = rng() % 100;
		std::shuffle(v.begin(), v.end(), rng);
	}
	// 获取群成员名称（优先使用群名片，其次使用昵称）
	inline void get_group_member_name(const ApiFunc& api, int64_t group_id, int64_t user_id, std::string& name) {
		const json response = api("get_group_member_info", json{{"group_id", group_id}, {"user_id", user_id}});
		const int64_t retcode = response["retcode"].get<int64_t>();
		if (retcode) {
			name = api("get_stranger_info", json{{"user_id", user_id}})["data"].at("nick").get<std::string>();
		}
		else {
			const auto group_member_info = response["data"];
			const std::string card = group_member_info.at("card").get<std::string>();
			name = card == "" ? group_member_info.at("nickname").get<std::string>() : card;
		}
	}
	// 纯文本
	inline void add_text_message(json& message, const std::string& text) {
		message.emplace_back(json{{"type", "text"}, {"data", json{{"text", text}}}});
	}
	// 图片
	inline void add_image_message(json& message, const std::string& file_path) {
		message.emplace_back(json{{"type", "image"}, {"data", json{{"file", file_path}}}});
	}
	// @某人
	inline void add_at_message(json& message, int64_t qq) {
		message.emplace_back(json{{"type", "at"}, {"data", json{{"qq", qq}}}});
	}
	// 回复
	inline void add_reply_message(json& message, int message_id) {
		message.emplace_back(json{{"type", "reply"}, {"data", json{{"id", message_id}}}});
	}
} // namespace common