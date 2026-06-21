#pragma once
#include <functional>
#include <nlohmann/json.hpp>
namespace fs = std::filesystem;
using json = nlohmann::json;
using ApiFunc = std::function<json(const std::string&, const json&)>;
