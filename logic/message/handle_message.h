#pragma once
#include <functional>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

class HandleMessage {
  public:
	static void start(const json& event, std::function<json(const std::string&, const json&)> api);
};