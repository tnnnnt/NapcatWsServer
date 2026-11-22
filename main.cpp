#include <fstream>
#include <iostream>
#include <string>
#include <thread>
#include "core/bot_client.h"
#include "network_types.hpp"
const std::string WORK_DIR = "/home/bot/qq_robot/"; // 工作目录
const std::string PORT_FILE = WORK_DIR + "port.txt"; // 端口配置文件路径
int main() {
	// 整体流程：读取监听端口 -> 建立 TCP 监听 -> 循环等待 Napcat（QQ 框架）连入，
	// 每连入一个机器人就起一个独立线程运行一个 BotClient。
	// 注意：这里是「被动等待连接」——是 Napcat 主动连过来，不是本程序去连 Napcat。

	// 1. 从 port.txt 读取监听端口号（文件里就一行数字）
	std::string port_str;
	std::ifstream ifs_port(PORT_FILE);
	if (!ifs_port.is_open()) {
		std::cerr << "无法打开配置文件: " << PORT_FILE << std::endl;
		return -1;
	}
	std::getline(ifs_port, port_str);
	ifs_port.close();
	const short unsigned int PORT = std::stoi(port_str);

	// 2. 建立 TCP 监听。io_context 是 Boost.Asio 的事件循环驱动，acceptor 负责监听端口。
	net::io_context ioc{1};
	tcp::acceptor acceptor{ioc, {tcp::v4(), PORT}};
	std::cout << "Listening on port " << PORT << "\n";

	// 3. 主循环：不断 accept 新的 TCP 连接，每来一个就交给一个独立线程处理。
	for (;;) {
		try {
			tcp::socket socket{ioc};
			// 阻塞等待一个客户端（机器人）连入；连入后 socket 即为该连接
			acceptor.accept(socket);

			// 每个连接起一个线程，lambda 捕获 socket（通过 move 转移所有权）
			std::thread([s = std::move(socket)]() mutable {
				try {
					// 在 TCP 连接之上做 WebSocket 握手升级（此 accept 与上面的 TCP accept 是两回事）
					websocket::stream<tcp::socket> ws(std::move(s));
					ws.accept();

					// 用升级后的 WebSocket 构造 BotClient 并启动，开始处理该机器人的消息
					BotClient bot(std::move(ws));
					bot.start();
				}
				catch (const std::exception& e) {
					std::cerr << "session error: " << e.what() << std::endl;
				}
			}).detach(); // detach：线程与主线程脱钩，自行运行、结束后自动回收（不可 join）
		}
		catch (const std::exception& e) {
			std::cerr << "accept error: " << e.what() << std::endl;
		}
	}
}
