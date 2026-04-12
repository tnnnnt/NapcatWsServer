#include "bot_client.h"
#include "../logic/command_router.h"
#include "../logic/common.hpp"
#include <iostream>
#include <chrono>
#include <cstdlib>

BotClient::BotClient(websocket::stream<tcp::socket>&& ws)
	: ws_(std::move(ws)) {
}

BotClient::~BotClient() {
	if (reader_.joinable())
		reader_.join();
	for (auto& t : workers_) {
		if (t.joinable())
			t.join();
	}
	if (sender_.joinable())
		sender_.join();
	if (schedule_midnight_task_.joinable())
		schedule_midnight_task_.join();
	if (schedule_periodic_task_.joinable())
		schedule_periodic_task_.join();
}

void BotClient::start() {
	CommandRouter::load_config();
	CommandRouter::load_today_group_member_message_number_data();
	reader_ = std::thread([this]() { read_loop(); });
	for (size_t i = 0; i < common::POOL_SIZE; ++i)
		workers_.emplace_back([this]() { worker_loop(); });
	//启动发送线程（限速核心）
	sender_ = std::thread([this]() { sender_loop(); });
	schedule_midnight_task_ = std::thread([this]() { schedule_midnight_task_loop(); });
	schedule_periodic_task_ = std::thread([this]() { schedule_periodic_task_loop(); });
	CommandRouter::updata_group_members_data([this](auto action, auto params) {
		return call_api(action, params);
		});
}

void BotClient::read_loop() {
	std::string msg;
	while (running_) {
		try {
			boost::beast::flat_buffer buffer;
			ws_.read(buffer);
			msg = boost::beast::buffers_to_string(buffer.data());
			json j = json::parse(msg);
			handle_post(j);
		}
		catch (std::exception& e) {
			std::cout << "read error: " << e.what() << std::endl;
			std::cout << "msg: \n" << msg << std::endl;
			running_ = false;
		}
	}
}

void BotClient::handle_post(const json& j) {
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
			if (msg["action"] == "send_private_msg" || msg["action"] == "send_group_msg" || msg["action"] == "send_msg") {
				//限速（核心）
				std::this_thread::sleep_for(std::chrono::seconds(common::BASE_DELAY + rand() % common::RANDOM_DELAY));
			}
			std::lock_guard<std::mutex> lock(ws_mutex_);
			ws_.write(boost::asio::buffer(msg.dump()));
		}
		catch (std::exception& e) {
			std::cout << "send error: " << e.what() << std::endl;
		}
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

void BotClient::schedule_midnight_task_loop() {
	while (true) {
		using namespace std::chrono;
		auto now = system_clock::now();
		std::time_t now_c = system_clock::to_time_t(now);
		std::tm local_tm{};
		localtime_r(&now_c, &local_tm);
		// 设置到明天 00:00:00
		local_tm.tm_hour = 0;
		local_tm.tm_min = 0;
		local_tm.tm_sec = 0;
		++local_tm.tm_mday;
		auto next_midnight = system_clock::from_time_t(mktime(&local_tm));
		auto sleep_duration = next_midnight - now;
		std::this_thread::sleep_for(sleep_duration);
		// 每日定时处理
		CommandRouter::daily([this](auto action, auto params) {
			return call_api(action, params);
			});
	}
}

void BotClient::schedule_periodic_task_loop() {
	while (true) {
		std::this_thread::sleep_for(std::chrono::seconds(common::TIME_SAVE_INTERVAL));
		CommandRouter::save_today_group_member_message_number_data();
	}
}