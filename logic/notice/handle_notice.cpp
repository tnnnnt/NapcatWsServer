#include "handle_notice.h"
#include <cstdlib>

void HandleNotice::start(const json& event, std::function<json(const std::string&, const json&)> api) {
	const std::string notice_type = event.at("notice_type").get<std::string>();
	if (notice_type == "group_upload") ;
	else if (notice_type == "group_admin") ;
	else if (notice_type == "group_decrease") ;
	else if (notice_type == "group_increase") {
		const auto group_id = event.at("group_id").get<int64_t>();// ÈººÅ
		const auto user_id = event.at("user_id").get<int64_t>();// ¼ÓÈëÕß QQ ºÅ
		json params{};
		params["group_id"] = group_id;
		json message = json::array();
		message.emplace_back(json{
			{"type", "at"},
			{"data", json{
				{"qq", user_id}
			}}
			});
		message.emplace_back(json{
			{"type", "text"},
			{"data", json{
				{"text", " »¶Ó­ß÷~°®Äãß÷~"}
			}}
			});
		params["message"] = message;
		api("send_group_msg", params);
	}
	else if (notice_type == "group_ban") ;
	else if (notice_type == "friend_add") ;
	else if (notice_type == "group_recall") ;
	else if (notice_type == "friend_recall") ;
	else if (notice_type == "notify") ;
}