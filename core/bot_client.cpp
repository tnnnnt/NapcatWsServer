#include "bot_client.h"
#include "../logic/command_router.h"
#include <iostream>

BotClient::BotClient(websocket::stream<tcp::socket>&& ws)
	: ws_(std::move(ws)) {
}

BotClient::~BotClient() {
	stop();
}

void BotClient::start() {
	reader_ = std::thread([this]() { read_loop(); });
	for (int i = 0; i < 4; ++i)
		workers_.emplace_back([this]() { worker_loop(); });
}

void BotClient::stop() {
	if (reader_.joinable())
		reader_.join();
	for (auto& t : workers_) {
		if (t.joinable())
			t.join();
	}
}

void BotClient::read_loop() {
	while (true) {
		boost::beast::flat_buffer buffer;
		ws_.read(buffer);
		std::string msg = boost::beast::buffers_to_string(buffer.data());
		json j = json::parse(msg);
		handle_message(j);
	}
}

void BotClient::handle_message(const json& j) {
	if (j.contains("echo")) {
		std::lock_guard<std::mutex> lock(api_mutex_);
		auto it = pending_.find(j["echo"]);
		if (it != pending_.end()) {
			it->second.set_value(j);
			pending_.erase(it);
		}
		return;
	}
	if (j.contains("post_type")) {
		std::lock_guard<std::mutex> lock(queue_mutex_);
		event_queue_.push(j);
		cv_.notify_one();
	}
}

void BotClient::worker_loop() {
	while (true) {
		json event;
		{
			std::unique_lock<std::mutex> lock(queue_mutex_);
			cv_.wait(lock, [this] { return !event_queue_.empty(); });
			event = event_queue_.front();
			event_queue_.pop();
		}
		CommandRouter::handle(event, [this](auto action, auto params) {
			return call_api(action, params);
			});
	}
}

void BotClient::send_json(const json& j) {
	std::lock_guard<std::mutex> lock(send_mutex_);
	ws_.write(boost::asio::buffer(j.dump()));
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
	send_json(req);
	return f.get();
}