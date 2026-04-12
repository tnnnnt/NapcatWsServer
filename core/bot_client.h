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
#include <atomic>

using json = nlohmann::json;
namespace websocket = boost::beast::websocket;
using tcp = boost::asio::ip::tcp;

class BotClient {
public:
	BotClient(websocket::stream<tcp::socket>&& ws);
	~BotClient();
	void start();

private:
	websocket::stream<tcp::socket> ws_;

	//线程
	std::thread reader_;
	std::vector<std::thread> workers_;
	std::thread sender_;
	std::thread schedule_midnight_task_;
	std::thread schedule_periodic_task_;

	std::atomic<bool> running_{ true };

	//事件队列
	std::queue<json> event_queue_;
	std::mutex queue_mutex_;
	std::condition_variable cv_;

	//API响应管理
	std::map<std::string, std::promise<json>> pending_;
	std::mutex api_mutex_;

	//发送队列（限速核心）
	std::queue<json> send_queue_;
	std::mutex send_queue_mutex_;
	std::condition_variable send_cv_;

	//WebSocket写锁（必须）
	std::mutex ws_mutex_;

	int echo_id_ = 0;

	void read_loop();
	void worker_loop();
	void sender_loop();
	void handle_post(const json& j);
	void enqueue_send(const json& j);
	std::string gen_echo();
	json call_api(const std::string&, const json&);
	void schedule_midnight_task_loop();
	void schedule_periodic_task_loop();
};