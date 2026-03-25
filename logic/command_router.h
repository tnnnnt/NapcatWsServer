#pragma once
#include <nlohmann/json.hpp>
#include <functional>

using json = nlohmann::json;

class CommandRouter {
public:
	using ApiFunc = std::function<json(const std::string&, const json&)>;

	static void handle(const json& event, ApiFunc api);
};