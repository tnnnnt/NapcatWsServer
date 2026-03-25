#pragma once
#include <nlohmann/json.hpp>
#include <functional>

using json = nlohmann::json;

class HandleNotice {
public:
    static void start(const json& event, std::function<json(const std::string&, const json&)> api);
};