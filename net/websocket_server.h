#pragma once
#include <boost/beast.hpp>
#include <boost/asio.hpp>
#include <functional>

namespace beast = boost::beast;
namespace websocket = beast::websocket;
namespace net = boost::asio;
using tcp = net::ip::tcp;

class WebSocketServer {
public:
	using SessionHandler = std::function<void(websocket::stream<tcp::socket>)>;

	WebSocketServer(int port);

	void start(SessionHandler handler) const;

private:
	int port_;
};