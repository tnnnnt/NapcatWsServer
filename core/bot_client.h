#pragma once
#include <boost/beast.hpp>
#include <boost/asio.hpp>
#include <nlohmann/json.hpp>
#include <thread>
#include <mutex>
#include <queue>
#include <condition_variable>
#include <map>
#include <future>

using json = nlohmann::json;
namespace websocket = boost::beast::websocket;
using tcp = boost::asio::ip::tcp;

class BotClient {
public:
	BotClient(websocket::stream<tcp::socket>&& ws);
	~BotClient();
	void start();
	void stop();

private:
	websocket::stream<tcp::socket> ws_;

	std::thread reader_;
	std::vector<std::thread> workers_;

	std::mutex send_mutex_;

	std::queue<json> event_queue_;
	std::mutex queue_mutex_;
	std::condition_variable cv_;

	std::map<std::string, std::promise<json>> pending_;
	std::mutex api_mutex_;

	int echo_id_ = 0;

	void read_loop();
	void worker_loop();
	void handle_message(const json& j);

	void send_json(const json& j);
	std::string gen_echo();

	json call_api(const std::string&, const json&);
};