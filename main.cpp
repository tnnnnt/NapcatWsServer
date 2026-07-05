#include <fstream>
#include <iostream>
#include <string>
#include <thread>
#include "core/bot_client.h"
#include "network_types.hpp"
const std::string WORK_DIR = "/home/bot/qq_robot/"; // 工作目录
const std::string PORT_FILE = WORK_DIR + "port.txt";
int main() {
	std::string port_str;
	std::ifstream ifs_port(PORT_FILE);
	if (!ifs_port.is_open()) {
		std::cerr << "无法打开配置文件: " << PORT_FILE << std::endl;
		return -1;
	}
	std::getline(ifs_port, port_str);
	ifs_port.close();
	const short unsigned int PORT = std::stoi(port_str);

	net::io_context ioc{1};
	tcp::acceptor acceptor{ioc, {tcp::v4(), PORT}};
	std::cout << "Listening on port " << PORT << "\n";
	for (;;) {
		try {
			tcp::socket socket{ioc};
			acceptor.accept(socket);

			std::thread([s = std::move(socket)]() mutable {
				try {
					websocket::stream<tcp::socket> ws(std::move(s));
					ws.accept();

					BotClient bot(std::move(ws));
					bot.start();
				}
				catch (const std::exception& e) {
					std::cerr << "session error: " << e.what() << std::endl;
				}
			}).detach();
		}
		catch (const std::exception& e) {
			std::cerr << "accept error: " << e.what() << std::endl;
		}
	}
}