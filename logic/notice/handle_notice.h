#pragma once
#include <functional>
#include <nlohmann/json.hpp>
#include "../logic_types.hpp"
class HandleNotice {
  public:
	static void start(const json& event, const ApiFunc& api);
};