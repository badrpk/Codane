#include "codane/chat_tui.hpp"
#include "codane/workspace.hpp"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <iostream>
#include <sstream>
#include <unistd.h>
namespace codane {
namespace {
std::string trim(const std::string& s){auto a=s.find_first_not_of(" \t\r\n");if(a==std::string::npos)return {};return s.substr(a,s.find_last_not_of(" \t\r\n")-a+1);}
std::string id(){static std::atomic<unsigned> n{0};return std::to_string(std::chrono::system_clock::now().time_since_epoch().count())+"-"+std::to_string(getpid())+"-"+std::to_string(n++);}
std::string display(const std::string& s){std::string result;for(unsigned char c:s){if(c==27||c==127||(c<32&&c!='\n'&&c!='\t'))result+='?';else result+=char(c);}return result;}
}
ChatCommand parse_chat_command(const std::string& line){auto s=trim(line);if(s.empty()||s[0]!='/')return {};auto end=s.find_first_of(" \t");return {s.substr(1,end==std::string::npos?end:end-1),end==std::string::npos?"":trim(s.substr(end))};}
ConversationSession ChatTui::fresh(){return ConversationSession::create(chats_,{id(),workspace_,config_.provider,config_.model,{},{}});}
ChatTui::ChatTui(ChatConfig c,std::filesystem::path workspace,std::filesystem::path chats,std::istream& in,std::ostream& out):config_(std::move(c)),workspace_(WorkspacePolicy::select(workspace).root()),chats_(std::move(chats)),in_(in),out_(out),registry_(config_.registry()),session_(fresh()){}
void ChatTui::status(){out_<<"Workspace: "<<display(workspace_.string())<<"\nProvider: "<<display(session_.metadata().provider_policy)<<"\nModel: "<<display(session_.metadata().model_policy)<<"\nSession: "<<session_.metadata().session_id<<'\n';}
std::string ChatTui::select(const std::vector<std::string>& choices,const std::string& label){
 out_<<label<<'\n';for(size_t i=0;i<choices.size();++i)out_<<i+1<<") "<<display(choices[i])<<'\n';out_<<"Number (Enter cancels): "<<std::flush;std::string line;if(!std::getline(in_,line)||trim(line).empty())return {};
 try{size_t used=0;auto n=std::stoul(trim(line),&used);if(used==trim(line).size()&&n>0&&n<=choices.size())return choices[n-1];}catch(...){}out_<<"Invalid selection\n";return {};
}
void ChatTui::workspace_browser(){
 auto current=workspace_;
 for(;;){std::vector<std::filesystem::path> dirs;std::error_code ec;for(std::filesystem::directory_iterator it(current,ec),end;!ec&&it!=end;it.increment(ec))if(it->is_directory(ec))dirs.push_back(it->path());std::sort(dirs.begin(),dirs.end());
  out_<<"Folder: "<<display(current.string())<<"\n0) Select this folder\n.. Parent | N Create folder | Q Cancel\n";for(size_t i=0;i<dirs.size();++i)out_<<i+1<<") "<<display(dirs[i].filename().string())<<'\n';out_<<"Folder > "<<std::flush;std::string line;if(!std::getline(in_,line))return;line=trim(line);
  if(line=="q"||line=="Q")return;
  if(line==".."||line=="\x1b[D"||line=="\b"||line=="\x7f"){current=current.parent_path();continue;}
  if(line=="0"){auto chosen=WorkspacePolicy::select(current).root();if(chosen==workspace_)return;out_<<"Switch workspace to "<<display(chosen.string())<<"? [y/N] "<<std::flush;if(!std::getline(in_,line)||line!="y")return;workspace_=chosen;config_=ChatConfig::load(workspace_);registry_=config_.registry();session_=fresh();status();return;}
  if(line=="n"||line=="N"){out_<<"New folder name: "<<std::flush;if(!std::getline(in_,line))return;line=trim(line);if(line.empty()||line=="."||line==".."||line.find('/')!=std::string::npos||line.find('\\')!=std::string::npos){out_<<"Use a simple folder name\n";continue;}auto child=WorkspacePolicy::select(current).require_inside(line);if(std::filesystem::create_directory(child,ec))current=child;else out_<<"Cannot create folder\n";continue;}
  try{size_t used=0;auto n=std::stoul(line,&used);if(used==line.size()&&n>0&&n<=dirs.size())current=std::filesystem::canonical(dirs[n-1]);else out_<<"Invalid folder selection\n";}catch(...){out_<<"Invalid folder selection\n";}
 }
}
bool ChatTui::command(const ChatCommand& c){
 if(c.name=="exit")return false;
 if(c.name=="help")out_<<"Type a coding request. Commands:\n/workspace /folder /provider /providers /model /models /new /history /status /config /help /exit\n";
 else if(c.name=="status")status();
 else if(c.name=="providers"){for(const auto& p:registry_.list())out_<<display(p.id)<<" — "<<display(p.display_name)<<" ("<<(p.configured?"enabled":"disabled")<<", "<<(p.available?"executable found":"not found")<<")\n";}
 else if(c.name=="provider"){auto choice=c.argument;if(choice.empty()){std::vector<std::string> choices{"auto"};for(auto& p:registry_.list())choices.push_back(p.id);choice=select(choices,"Choose provider");}if(!choice.empty()){if(choice!="auto"&&!registry_.get(choice))out_<<"Unknown provider\n";else{session_.update_selection(choice,session_.metadata().model_policy);config_.provider=choice;status();}}}
 else if(c.name=="model"){auto model=c.argument;if(model.empty()){out_<<"Model ID (auto for harness default, Enter cancels): "<<std::flush;std::getline(in_,model);model=trim(model);}if(!model.empty()){session_.update_selection(session_.metadata().provider_policy,model);config_.model=model;status();}}
 else if(c.name=="models")out_<<"auto — installed harness default\nUse /model <provider-supported model ID>. Remote model catalogs arrive in a later phase.\n";
 else if(c.name=="new"){session_=fresh();out_<<"New conversation\n";status();}
 else if(c.name=="workspace"||c.name=="folder")workspace_browser();
 else if(c.name=="history"){
  std::vector<std::string> ids;for(const auto& entry:std::filesystem::directory_iterator(chats_))if(entry.is_directory()&&std::filesystem::exists(entry.path()/"session.json"))ids.push_back(entry.path().filename().string());std::sort(ids.rbegin(),ids.rend());
  for(const auto& s:ids)out_<<s<<'\n';
  if(!c.argument.empty()){auto restored=ConversationSession::load(chats_,c.argument);auto root=WorkspacePolicy::select(restored.metadata().workspace).root();if(root!=workspace_){out_<<"Reopen workspace "<<display(root.string())<<"? [y/N] "<<std::flush;std::string answer;if(!std::getline(in_,answer)||answer!="y")return true;}
   workspace_=root;config_=ChatConfig::load(workspace_);config_.provider=restored.metadata().provider_policy;config_.model=restored.metadata().model_policy;registry_=config_.registry();session_=std::move(restored);status();for(const auto& m:session_.messages())out_<<m.role<<" > "<<display(m.content)<<'\n';
  }else out_<<"Reopen with /history <session-id>\n";
 }
 else if(c.name=="config"){out_<<"User configuration: "<<display(chat_user_config().string())<<"\nWorkspace configuration: "<<display((workspace_/".codane/config.json").string())<<"\nUse JSON provider/model and harnesses entries (executable, argv, extra_argv, enabled, timeout_ms, max_stdout, max_stderr).\n/config reload applies files; /provider and /model select this session.\n";if(c.argument=="reload"){auto updated=ChatConfig::load(workspace_);config_=std::move(updated);registry_=config_.registry();session_.update_selection(config_.provider,config_.model);status();}}
 else out_<<"Unknown command. Use /help.\n";
 return true;
}
int ChatTui::run(){
 out_<<"CODANE\n";status();out_<<"Type a coding request, or /help.\n";
 for(std::string line;out_<<"\nYou > "<<std::flush,std::getline(in_,line);){if(trim(line).empty())continue;try{
  if(trim(line)[0]=='/'){if(!command(parse_chat_command(line)))break;continue;}
  ChatRequest request{workspace_,session_.messages(),line,session_.metadata().model_policy};session_.append_message({"user",line});std::string previous;
  auto result=ProviderRouter(registry_).send(request,session_.metadata().provider_policy,{},config_.explicit_fallback,[&](const std::string& provider,const ChatResult* r){
   if(!r){auto p=registry_.get(provider);out_<<"Codane > Using "<<display(p?p->info().display_name:provider)<<'\n'<<std::flush;session_.append_transition({previous,provider,previous.empty()?"selected":"safe fallback"});previous=provider;}
   else if(!r->ok)session_.append_error({failure_name(r->failure),"Provider request failed"});
  });
  if(result.ok){session_.append_message({"assistant",result.text});out_<<"Codane > "<<display(result.text)<<'\n';}
  else{out_<<"Codane > "<<failure_name(result.failure)<<'\n';if(result.workspace_may_have_changed)out_<<"Workspace may have changed. Automatic fallback stopped; inspect it before retrying.\n";}
 }catch(const std::exception&){out_<<"Codane > Unable to complete this operation. Check workspace, configuration and session files.\n";}}
 return 0;
}
int run_chat_tui(){try{auto workspace=std::filesystem::current_path();ChatTui tui(ChatConfig::load(workspace),workspace,chat_state_root(),std::cin,std::cout);return tui.run();}catch(const std::exception&){std::cerr<<"codane: cannot initialize chat workspace/configuration/storage\n";return 2;}}
}
