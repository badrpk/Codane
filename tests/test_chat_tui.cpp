#include "codane/chat_tui.hpp"
#include <sstream>
#include <stdexcept>
#include <iostream>
using namespace codane;
int main(){auto dir=std::filesystem::current_path()/"tui-fixture";std::filesystem::create_directories(dir);auto cfg=ChatConfig::load(dir,dir/"no-user-config");for(auto&[id,c]:cfg.harnesses){c.enabled=id=="custom";c.executable="/bin/echo";if(id=="custom")c.prompt_on_stdin=false;}cfg.provider="custom";std::istringstream in("/provider custom\n/model literal-model\nhello world\n/status\n/providers\n/models\n/config\n/help\n/history\n/new\n/exit\n");std::ostringstream out;ChatTui ui(cfg,dir,dir/"chats",in,out);if(ui.run()!=0)throw std::runtime_error("chat UI exit");auto text=out.str();if(text.find("You >")==std::string::npos||text.find("Using Custom harness")==std::string::npos||text.find("hello world")==std::string::npos||text.find("/workspace")==std::string::npos)throw std::runtime_error("chat flow");auto command=parse_chat_command("/provider   custom ");if(command.name!="provider"||command.argument!="custom")throw std::runtime_error("slash parsing");std::filesystem::remove_all(dir);std::cout<<"chat UI tests passed\n";}
