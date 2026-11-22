#pragma once
#include <condition_variable>
#include <future>
#include <map>
#include <mutex>
#include <queue>
#include <thread>
#include "../network_types.hpp"

// 机器人客户端：对应一个 WebSocket 连接（一个机器人）。
// 内部通过多线程流水线处理消息：
//   读线程收包 -> 事件队列 -> 线程池处理 -> 发送队列 -> 发送线程限速发包
// 只保留核心三线程（reader / workers / sender），业务逻辑由 CommandRouter 承载。
class BotClient {
  public:
	BotClient(websocket::stream<tcp::socket>&& ws);
	~BotClient();
	void start(); // 启动所有内部线程

  private:
	websocket::stream<tcp::socket> ws_; // 该连接对应的 WebSocket 流
	// ===== WebSocket 写锁（必须） =====
	// 同一时刻只能有一个线程写 WebSocket，否则数据会交错
	std::mutex ws_mutex_;

	// ===== 内部线程 =====
	std::thread reader_;			   // 读线程：从 WebSocket 收 JSON
	std::vector<std::thread> workers_; // 工作线程池：处理业务事件（数量由 kPoolSize 决定）
	std::thread sender_;			   // 发送线程：限速后统一发消息

	// ===== 事件队列（读线程 -> 工作线程） =====
	std::queue<json> event_queue_;	   // 待处理的事件
	std::mutex event_queue_mutex_;	   // 保护 event_queue_
	std::condition_variable event_cv_; // 用于通知工作线程「有新事件了」

	// ===== API 响应管理（echo 关联） =====
	// 发送 API 请求时按 echo 存一个 promise，收到回包时用 echo 找到并 fulfill
	std::map<std::string, std::promise<json>> pending_;
	std::mutex pending_mutex_; // 保护 pending_

	// ===== 发送队列（限速核心） =====
	std::queue<json> send_queue_;	  // 待发送的请求
	std::mutex send_queue_mutex_;	  // 保护 send_queue_
	std::condition_variable send_cv_; // 用于通知发送线程「有消息要发了」

	int echo_id_ = 0; // echo 自增序号，用于给每个 API 请求生成唯一标识

	// ===== 内部方法 =====
	void read_loop();								// 读线程主循环：收包并分发（API 回包 or 事件）
	void worker_loop();								// 工作线程主循环：从事件队列取事件交给业务处理
	void sender_loop();								// 发送线程主循环：限速 + 抖动后写 WebSocket
	void handle_post(const json& j);				// 判断收到的 JSON 是「API 回包」还是「事件」
	void enqueue_send(const json& j);				// 入发送队列
	std::string gen_echo();							// 生成唯一 echo
	json call_api(const std::string&, const json&); // 发起 API 调用并阻塞等待回包
};
