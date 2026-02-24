#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/json.hpp>
#include <filesystem>
#include <iostream>
#include <thread>
#include <vector>
#include <windows.h>

namespace fs = std::filesystem;
namespace json = boost::json;
namespace beast = boost::beast;
namespace http = beast::http;
namespace websocket = beast::websocket;
namespace net = boost::asio;
namespace ssl = net::ssl;
using tcp = net::ip::tcp;

const int64_t ROBOT_QQ = 3777014797; // 替换为你的机器人 QQ 号
const std::string WORK_DIR = "C:/QQRobot"; // 替换为你的工作目录路径
const std::string EAT_DIR = WORK_DIR + "/eat"; // 吃什么

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
			files.push_back(entry.path().filename().string());
		}
	}
}

static std::string remove_extension(const std::string& filename) {
	const std::size_t pos = filename.find_last_of('.');
	if (pos == std::string::npos)
		return filename;  // 没有后缀
	return filename.substr(0, pos);
}

// 做出响应
static void reply(const std::string& action, const json::object& params, websocket::stream<tcp::socket>& ws) {
	json::object reply;
	reply["action"] = action;
	reply["params"] = params;
	ws.write(net::buffer(json::serialize(reply)));
}

// 处理私聊消息
static void handle_private_message(const json::object& obj, websocket::stream<tcp::socket>& ws) {
	json::object params;
	params["user_id"] = obj.at("user_id");
	params["message"] = "暂时不支持私聊，请催促tnt更新";
	reply("send_private_msg", params, ws);
}

// 处理群消息
static void handle_group_message(const json::object& obj, websocket::stream<tcp::socket> &ws) {
	const auto time = obj.at("time").as_int64();
	const auto group_id = obj.at("group_id").as_int64();
	const auto user_id = obj.at("user_id").as_int64();
	const auto& message_array = obj.at("message").as_array();
	const auto message_size = message_array.size();
	if (message_size == 1) {
		const auto& seg_obj = message_array[0].as_object();
		const std::string type = seg_obj.at("type").as_string().c_str();
		if (type == "text") {
			const std::string text = seg_obj.at("data").as_object().at("text").as_string().c_str();
			if (text == "/今日运势")
			{
				json::object params;
				params["group_id"] = group_id;
				json::array message;
				std::vector<std::string> fortune = {"出行", "交友", "恋爱", "相亲", "工作", "面试", "VRChat", "游戏",
					"学习", "睡觉", "吃饭", "喝水", "运动", "理财", "打扫卫生", "写代码", "unity", "摸鱼", "买彩票", "请客",
					"旅行", "结婚", "生孩子", "搬家", "买东西", "看电影", "看书", "水群", "吃瓜"}; // 运势
				const int64_t seed = time / 86400 + user_id; // 每天每人一个固定的 seed
				int luckey_num;
				shuffle_vector(fortune, luckey_num, seed);
				message.emplace_back(json::object{
					{"type", "at"},
					{"data", json::object{
						{"qq", user_id}
					}}
					});
				message.emplace_back(json::object{
					{"type", "text"},
					{"data", json::object{
						{"text", "\n今日运势（仅供娱乐）\n宜：" + fortune[0] + " " + fortune[1] + " " + fortune[2] +
						"\n忌：" + fortune[3] + " " + fortune[4] + " " + fortune[5] +"\n幸运数字：" + std::to_string(luckey_num)}
					}}
					});
				params["message"] = message;
				reply("send_group_msg", params, ws);
			}
			else if (text.find("吃什么") != std::string::npos) {
				std::vector<std::string> files;
				get_files(EAT_DIR, files);
				json::object params;
				params["group_id"] = group_id;
				json::array message;
				if (files.empty()) {
					message.emplace_back(json::object{
						{"type", "text"},
						{"data", json::object{
							{"text", "别吃"}
						}}
						});
				}
				else {
					const std::string random_file = files[time % files.size()];
					message.emplace_back(json::object{
						{"type", "text"},
						{"data", json::object{
							{"text", "推荐" + remove_extension(random_file)}
						}}
						});
					message.emplace_back(json::object{
						{"type", "image"},
						{"data", json::object{
							{"file", "file:///" + EAT_DIR + "/" + random_file}
						}}
						});
				}
				params["message"] = message;
				reply("send_group_msg", params, ws);
			}
		}
	}
}

// 处理消息事件，包括私聊消息、群消息等
static void handle_message(const json::object& obj, websocket::stream<tcp::socket>& ws) {
	const std::string message_type = obj.at("message_type").as_string().c_str();
	std::cout << "message_type: " << message_type << std::endl;
	if (message_type == "private") handle_private_message(obj, ws);
	else if (message_type == "group") handle_group_message(obj, ws);
}

// 处理群文件上传事件
static void handle_group_upload_notice(const json::object& obj, websocket::stream<tcp::socket>& ws) {
}

// 处理群管理员变动事件
static void handle_group_admin_notice(const json::object& obj, websocket::stream<tcp::socket>& ws) {
}

// 处理群成员减少事件
static void handle_group_decrease_notice(const json::object& obj, websocket::stream<tcp::socket>& ws) {
}

// 处理群成员增加事件
static void handle_group_increase_notice(const json::object& obj, websocket::stream<tcp::socket>& ws) {
	const auto group_id = obj.at("group_id").as_int64();// 群号
	const auto user_id = obj.at("user_id").as_int64();// 加入者 QQ 号
	json::object params;
	params["group_id"] = group_id;
	json::array message;
	message.emplace_back(json::object{
		{"type", "at"},
		{"data", json::object{
			{"qq", user_id}
		}}
		});
	message.emplace_back(json::object{
		{"type", "text"},
		{"data", json::object{
			{"text", " 欢迎喵~"}
		}}
		});
	params["message"] = message;
	reply("send_group_msg", params, ws);
}

// 处理群禁言事件
static void handle_group_ban_notice(const json::object& obj, websocket::stream<tcp::socket>& ws) {
}

// 处理好友添加事件
static void handle_friend_add_notice(const json::object& obj, websocket::stream<tcp::socket>& ws) {
}

// 处理群消息撤回事件
static void handle_group_recall_notice(const json::object& obj, websocket::stream<tcp::socket>& ws) {
}

// 处理好友消息撤回事件
static void handle_friend_recall_notice(const json::object& obj, websocket::stream<tcp::socket>& ws) {
}

// 处理群内戳一戳、群红包运气王、群成员荣誉变更等事件
static void handle_notify_notice(const json::object& obj, websocket::stream<tcp::socket>& ws) {
}

// 处理通知事件，包括群成员变动、好友变动等
static void handle_notice(const json::object& obj, websocket::stream<tcp::socket>& ws) {
	const std::string notice_type = obj.at("notice_type").as_string().c_str();
	std::cout << "notice_type: " << notice_type << std::endl;
	if (notice_type == "group_upload") handle_group_upload_notice(obj, ws);
	else if (notice_type == "group_admin") handle_group_admin_notice(obj, ws);
	else if (notice_type == "group_decrease") handle_group_decrease_notice(obj, ws);
	else if (notice_type == "group_increase") handle_group_increase_notice(obj, ws);
	else if (notice_type == "group_ban") handle_group_ban_notice(obj, ws);
	else if (notice_type == "friend_add") handle_friend_add_notice(obj, ws);
	else if (notice_type == "group_recall") handle_group_recall_notice(obj, ws);
	else if (notice_type == "friend_recall") handle_friend_recall_notice(obj, ws);
	else if (notice_type == "notify") handle_notify_notice(obj, ws);
}

// 处理请求事件，包括加群请求、加好友请求等
static void handle_request(const json::object& obj, websocket::stream<tcp::socket>& ws) {
}

// 处理元事件，包括 OneBot 生命周期、心跳等
static void handle_meta_event(const json::object& obj, websocket::stream<tcp::socket>& ws) {
}

// 处理事件
static void handle_event(const json::object& obj, websocket::stream<tcp::socket>& ws) {
	const std::string post_type = obj.if_contains("post_type") ? obj.at("post_type").as_string().c_str() : "";
	if (post_type == "message") handle_message(obj, ws);
	else if (post_type == "notice") handle_notice(obj, ws);
	else if (post_type == "request") handle_request(obj, ws);
	else if (post_type == "meta_event") handle_meta_event(obj, ws);
}

// 客户端会话处理
static void do_session(tcp::socket socket) {
	try {
		websocket::stream<tcp::socket> ws(std::move(socket));
		// 接受握手
		ws.accept();
		std::cout << "WebSocket connected\n";
		for (;;) {
			beast::flat_buffer buffer;
			ws.read(buffer);
			const std::string msg = beast::buffers_to_string(buffer.data());
			const json::value jv = json::parse(msg);
			const auto& obj = jv.as_object();
			handle_event(obj, ws);
		}
	}
	catch (std::exception const& e) {
		std::cout << "Session ended: " << e.what() << "\n";
	}
}

int main() {
	try {
		net::io_context ioc{ 1 };
		tcp::acceptor acceptor{ ioc, {tcp::v4(), 8080} };
		std::cout << "Listening on 0.0.0.0:8080\n";
		for (;;) {
			tcp::socket socket{ ioc };
			acceptor.accept(socket);

			std::thread(
				[s = std::move(socket)]() mutable {
					do_session(std::move(s));
				}
			).detach();
		}
	}
	catch (std::exception const& e) {
		std::cerr << "Error: " << e.what() << "\n";
		return 1;
	}
}
