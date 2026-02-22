#include <boost/asio.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/json.hpp>
#include <iostream>
#include <thread>
#include <windows.h>
#include <boost/beast/http.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/asio/ssl.hpp>

namespace json = boost::json;
namespace beast = boost::beast;
namespace http = beast::http;
namespace websocket = beast::websocket;
namespace net = boost::asio;
namespace ssl = net::ssl;
using tcp = net::ip::tcp;

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
	const auto& message_array = obj.at("message").as_array();
	const auto message_len = message_array.size();
	if (!message_len) return;
	if (message_len == 1) {
		const auto& seg_obj = message_array[0].as_object();
		const std::string type = seg_obj.at("type").as_string().c_str();
		if (type == "text") {
			const std::string text = seg_obj.at("data").as_object().at("text").as_string().c_str();
			std::cout << "text: " << text << std::endl;
			if (text == "你好") {
				std::cout << "text is 你好" << std::endl;
				json::object params;
				params["group_id"] = obj.at("group_id");
				params["message"] = "你好";
				reply("send_group_msg", params, ws);
			}
		}
		else if (type == "face") {
		}
		// 其他消息类型可以在这里继续添加处理逻辑
	}
}

// 处理消息事件，包括私聊消息、群消息等
static void handle_message(const json::object& obj, websocket::stream<tcp::socket>& ws) {
	const std::string message_type = obj.at("message_type").as_string().c_str();
	std::cout << "message_type: " << message_type << std::endl;
	if (message_type == "private") handle_private_message(obj, ws);
	else if (message_type == "group") handle_group_message(obj, ws);
}

// 处理通知事件，包括群成员变动、好友变动等
static void handle_notice(const json::object& obj, websocket::stream<tcp::socket>& ws) {
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
