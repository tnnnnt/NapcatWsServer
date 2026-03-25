#include "websocket_server.h"
#include <thread>
#include <iostream>

WebSocketServer::WebSocketServer(int port) : port_(port) {}

void WebSocketServer::start(SessionHandler handler) const {

	net::io_context ioc{ 1 };
	tcp::acceptor acceptor{ ioc, {tcp::v4(), port_} };

	std::cout << "Listening on port " << port_ << std::endl;

	for (;;) {
		tcp::socket socket{ ioc };
		acceptor.accept(socket);

		std::thread([handler, s = std::move(socket)]() mutable {
			websocket::stream<tcp::socket> ws(std::move(s));
			ws.accept();
			handler(std::move(ws));
			}).detach();
	}
}