#pragma once
#include "codane/chat_config.hpp"
#include "codane/conversation.hpp"
#include <istream>
#include <ostream>
namespace codane {
struct ChatCommand {std::string name,argument;};
ChatCommand parse_chat_command(const std::string&);
class ChatTui {
 ChatConfig config_;std::filesystem::path workspace_,chats_;std::istream& in_;std::ostream& out_;
 ProviderRegistry registry_;ConversationSession session_;
 ConversationSession fresh();
 void status();
 std::string select(const std::vector<std::string>&,const std::string&);
 void workspace_browser();
 bool command(const ChatCommand&);
public:
 ChatTui(ChatConfig,std::filesystem::path workspace,std::filesystem::path chats,std::istream&,std::ostream&);
 int run();
};
int run_chat_tui();
}
