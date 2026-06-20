#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <iostream>
#include <thread>
#include "core/bot_client.h"
namespace net = boost::asio;
using tcp = net::ip::tcp;
constexpr int PORT = 8080;
int main() {
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