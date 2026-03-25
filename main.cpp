#include "net/websocket_server.h"
#include "core/bot_client.h"

int main() {
	WebSocketServer server(8080);
	server.start([](auto ws) {
		BotClient bot(std::move(ws));
		bot.start();
		});
	return 0;
}