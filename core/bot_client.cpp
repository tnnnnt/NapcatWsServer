#include "bot_client.h"
#include "../logic/command_router.h"
#include <iostream>
#include <chrono>
#include <cstdlib>

BotClient::BotClient(websocket::stream<tcp::socket>&& ws)
	: ws_(std::move(ws)) {
}

void BotClient::start() {
	reader_ = std::thread([this]() { read_loop(); });
	for (int i = 0; i < 4; ++i)
		workers_.emplace_back([this]() { worker_loop(); });
	//启动发送线程（限速核心）
	sender_ = std::thread([this]() { sender_loop(); });
}

void BotClient::read_loop() {
	while (running_) {
		try {
			boost::beast::flat_buffer buffer;
			ws_.read(buffer);
			std::string msg = boost::beast::buffers_to_string(buffer.data());
			json j = json::parse(msg);
			handle_message(j);
		}
		catch (std::exception& e) {
			std::cout << "read error: " << e.what() << std::endl;
			running_ = false;
		}
	}
}

void BotClient::handle_message(const json& j) {
	//API响应
	if (j.contains("echo")) {
		std::lock_guard<std::mutex> lock(api_mutex_);
		auto it = pending_.find(j["echo"]);
		if (it != pending_.end()) {
			it->second.set_value(j);
			pending_.erase(it);
		}
		return;
	}
	//事件
	if (j.contains("post_type")) {
		std::lock_guard<std::mutex> lock(queue_mutex_);
		event_queue_.push(j);
		cv_.notify_one();
	}
}

void BotClient::worker_loop() {
	while (running_) {
		json event;
		{
			std::unique_lock<std::mutex> lock(queue_mutex_);
			cv_.wait(lock, [this] { return !event_queue_.empty() || !running_; });
			if (!running_) return;
			event = event_queue_.front();
			event_queue_.pop();
		}
		//业务处理（调用API会自动进入发送队列）
		CommandRouter::handle(event, [this](auto action, auto params) {
			return call_api(action, params);
			});
	}
}

//核心：发送线程 + 限速 + 抖动
void BotClient::sender_loop() {
	while (running_) {
		json msg;
		{
			std::unique_lock<std::mutex> lock(send_queue_mutex_);
			send_cv_.wait(lock, [this] {
				return !send_queue_.empty() || !running_;
				});
			if (!running_) return;
			msg = send_queue_.front();
			send_queue_.pop();
		}
		try {
			std::lock_guard<std::mutex> lock(ws_mutex_);
			ws_.write(boost::asio::buffer(msg.dump()));
		}
		catch (std::exception& e) {
			std::cout << "send error: " << e.what() << std::endl;
		}
		//限速（核心）
		const int base = 300;                 // 300ms
		const int jitter = rand() % 200;      // 0~200ms
		std::this_thread::sleep_for(
			std::chrono::milliseconds(base + jitter)
		);
	}
}

void BotClient::enqueue_send(const json& j) {
	{
		std::lock_guard<std::mutex> lock(send_queue_mutex_);
		send_queue_.push(j);
	}
	send_cv_.notify_one();
}

std::string BotClient::gen_echo() {
	return "e" + std::to_string(echo_id_++);
}

json BotClient::call_api(const std::string& action, const json& params) {
	std::string echo = gen_echo();
	json req = {
		{"action", action},
		{"params", params},
		{"echo", echo}
	};
	std::promise<json> p;
	auto f = p.get_future();
	{
		std::lock_guard<std::mutex> lock(api_mutex_);
		pending_[echo] = std::move(p);
	}
	//不直接发送 → 进入限速队列
	enqueue_send(req);
	return f.get();
}