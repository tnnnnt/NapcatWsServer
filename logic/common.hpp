#pragma once
#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <mutex>
#include <random>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace fs = std::filesystem;

namespace common
{
	inline constexpr int64_t ROBOT_QQ = 3777014797; // 替换为你的机器人 QQ 号
	inline const std::string WORK_DIR = "/home/bot/qq_robot"; // 替换为你的工作目录路径
	inline const std::string EAT_DIR = WORK_DIR + "/eat"; // 吃什么
	inline std::unordered_map<int64_t, std::vector<int64_t>> group_members; // 群成员列表
	inline std::mutex group_members_mutex; // 保护 group_members 的互斥锁
	// 判断字符串是否仅包含空格
	inline bool is_only_spaces(const std::string& s)
	{
		return !s.empty() && std::all_of(s.begin(), s.end(), [](char c) { return c == ' '; });
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
}