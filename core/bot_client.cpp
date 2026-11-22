#include "bot_client.h"
#include <chrono>
#include <iostream>
#include <random>
#include "../logic/command_router.h"

// 本文件实现 BotClient 的完整生命周期与消息流水线。
// 数据流向：read_loop 收包 -> handle_post 分流 -> 事件进 event_queue_ / API 回包 fulfill promise
//         -> worker_loop 取事件做业务（业务里调用 call_api） -> call_api 发请求进 send_queue_
//         -> sender_loop 限速后写 WebSocket。

// ===== 骨架常量（原为配置文件，现写死，便于后续按需扩展） =====
namespace {
	constexpr size_t kPoolSize = 4;	 // 工作线程池大小
	constexpr size_t kBaseDelay = 1; // 发送基础延迟（秒）
	constexpr size_t kRandomDelay = 2; // 发送随机抖动（秒），0 表示不加抖动
	constexpr size_t kReadRetryDelay = 5; // 读出错后的重试等待（秒）
} // namespace

BotClient::BotClient(websocket::stream<tcp::socket>&& ws) : ws_(std::move(ws)) {}

// 析构：等待所有内部线程结束。注意：本对象由 main.cpp 里 detached 的线程持有，
// 正常退出时各线程需能自行结束才能走到这里（否则可能永远 join 不回来）。
BotClient::~BotClient() {
	if (reader_.joinable()) {
		reader_.join();
	}
	for (auto& t : workers_) {
		if (t.joinable()) {
			t.join();
		}
	}
	if (sender_.joinable()) {
		sender_.join();
	}
}

void BotClient::start() {
	// 启动三个核心线程
	reader_ = std::thread([this]() { read_loop(); }); // 读线程
	for (size_t i = 0; i < kPoolSize; ++i) {
		workers_.emplace_back([this]() { worker_loop(); }); // 工作线程池
	}
	sender_ = std::thread([this]() { sender_loop(); }); // 发送线程（限速核心）
}

void BotClient::read_loop() {
	std::string msg;
	for (;;) {
		try {
			boost::beast::flat_buffer buffer;
			ws_.read(buffer); // 阻塞读取一条 WebSocket 消息
			msg = boost::beast::buffers_to_string(buffer.data());
			json j = json::parse(msg); // 解析为 JSON
			handle_post(j);			   // 分流处理
		}
		catch (std::exception& e) {
			// 读失败（连接断开/消息格式错误等）：打印错误，等待一段时间后重试
			std::cout << "read error: " << e.what() << std::endl;
			std::cout << "msg: \n" << msg << std::endl;
			std::this_thread::sleep_for(std::chrono::seconds(kReadRetryDelay));
		}
	}
}

void BotClient::handle_post(const json& j) {
	// 情况一：带 echo 字段 → 是之前某个 API 请求的回包
	if (j.contains("echo")) {
		std::lock_guard<std::mutex> lock(pending_mutex_);
		auto it = pending_.find(j["echo"]);
		if (it != pending_.end()) {
			it->second.set_value(j); // fulfill 对应的 promise，唤醒 call_api 里阻塞的 future.get()
			pending_.erase(it);
		}
		return;
	}
	// 情况二：带 post_type 字段 → 是 Napcat 推来的事件
	if (j.contains("post_type")) {
		std::lock_guard<std::mutex> lock(event_queue_mutex_);
		event_queue_.push(j);
		event_cv_.notify_one(); // 唤醒一个工作线程来处理
	}
}

void BotClient::worker_loop() {
	for (;;) {
		json event{};
		{
			std::unique_lock<std::mutex> lock(event_queue_mutex_);
			event_cv_.wait(lock, [this] { return !event_queue_.empty(); }); // 无事件时阻塞等待
			event = event_queue_.front();
			event_queue_.pop();
		}
		// 业务处理。注意：业务里调用 call_api 会阻塞等待回包，
		// 所以需要多个 worker 线程并行，避免一个事件卡住导致其他事件排队。
		CommandRouter::handle(event, [this](auto action, auto params) { return call_api(action, params); });
	}
}

// 核心：发送线程 + 限速 + 抖动
void BotClient::sender_loop() {
	// 随机数生成器（每连接独立，用 random_device 播种，避免所有连接抖动序列一致）
	std::mt19937 rng(std::random_device{}());
	for (;;) {
		json msg{};
		{
			std::unique_lock<std::mutex> lock(send_queue_mutex_);
			send_cv_.wait(lock, [this] { return !send_queue_.empty(); }); // 无消息时阻塞等待
			msg = send_queue_.front();
			send_queue_.pop();
		}
		try {
			// 限速：基础延迟 + 随机抖动，避免短时间大量发消息触发 QQ 风控
			const auto jitter = kRandomDelay > 0 ? rng() % kRandomDelay : 0;
			std::this_thread::sleep_for(std::chrono::seconds(kBaseDelay + jitter));
			std::lock_guard<std::mutex> lock(ws_mutex_); // 写锁：保证同一时刻只有一个线程写 WebSocket
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
	send_cv_.notify_one(); // 唤醒发送线程
}

std::string BotClient::gen_echo() {
	return "e" + std::to_string(++echo_id_); // echo 用 "e" + 自增序号，保证每次请求唯一
}

// 发起 API 调用并阻塞等待回包。
// 原理（echo 关联）：为每个请求生成唯一 echo -> 存入 pending_（promise）-> 进发送队列
// -> 阻塞在 future.get() -> read_loop 收到带相同 echo 的回包后 set_value 唤醒。
json BotClient::call_api(const std::string& action, const json& params) {
	std::string echo = gen_echo();
	json req = {{"action", action}, {"params", params}, {"echo", echo}};
	std::promise<json> p;
	auto f = p.get_future();
	{
		std::lock_guard<std::mutex> lock(pending_mutex_);
		pending_[echo] = std::move(p);
	}
	// 不直接发送，而是进入限速队列，由发送线程统一发
	enqueue_send(req);
	return f.get(); // 阻塞等待回包
}
