#include "command_router.h"
#include "notice/handle_notice.h"
#include "message/handle_message.h"
#include <string>

void CommandRouter::handle(const json& event, ApiFunc api) {
	const std::string post_type = event.at("post_type").get<std::string>();
	if (post_type == "message") HandleMessage::start(event, api);
	else if (post_type == "notice") HandleNotice::start(event, api);
	else if (post_type == "request") ;
	else if (post_type == "meta_event") ;
}
/*
今日运势
吃什么
干什么！
*/