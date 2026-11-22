#pragma once
#include "logic_types.hpp"

// 业务入口：事件分发。
// 目前只做最小可验证的 echo 回显——收到私聊消息后把原文发回给对方，
// 用于验证「收到事件 -> 处理 -> 发出消息」整条链路是否打通（暂不处理群聊，避免打扰）。
// 后续的业务功能从这里开始扩展。
class CommandRouter {
  public:
	// 事件分发入口：根据 post_type 路由到对应处理器
	static void handle(const json& event, const ApiFunc& api);
};
