#include "codane/harness_provider.hpp"
#include <cstdlib>
#include <sstream>
#include <unistd.h>
namespace codane {
HarnessConfig harness_preset(const std::string& id){
 HarnessConfig c;c.id=id;c.executable=id;
 if(id=="codex"){c.display_name="Codex CLI";c.argv={"exec","--json","-"};c.output=HarnessOutput::CodexJsonl;}
 else if(id=="claude"){c.display_name="Claude Code";c.argv={"--print"};}
 else if(id=="grok"){c.display_name="Grok CLI";c.argv={"--prompt"};c.prompt_on_stdin=false;}
 else if(id=="gemini"){c.display_name="Gemini CLI";c.argv={"--prompt"};c.prompt_on_stdin=false;}
 else if(id=="pi"){c.display_name="Pi";c.argv={"--print"};c.enabled=false;}
 else if(id=="custom"){c.display_name="Custom harness";c.executable="";c.enabled=false;}
 else throw std::invalid_argument("unknown harness");
 return c;
}
std::filesystem::path find_executable(const std::string& name){
 if(name.empty()||name.find('\0')!=std::string::npos)return {};
 auto usable=[](const std::filesystem::path& p){std::error_code ec;return std::filesystem::is_regular_file(p,ec)&&access(p.c_str(),X_OK)==0;};
 if(name.find('/')!=std::string::npos){if(usable(name))return std::filesystem::absolute(name);return {};}
 const char* path=std::getenv("PATH");std::istringstream dirs(path?path:"");std::string dir;
 while(std::getline(dirs,dir,':')){auto p=std::filesystem::path(dir.empty()?".":dir)/name;if(usable(p))return std::filesystem::absolute(p);}
 return {};
}
HarnessProvider::HarnessProvider(HarnessConfig c,std::function<ProcessResult(const ProcessSpec&)> e):config_(std::move(c)),execute_(std::move(e)){}
ProviderInfo HarnessProvider::info()const{return {config_.id,config_.display_name,ProviderKind::Harness,config_.enabled&&!config_.executable.empty(),!find_executable(config_.executable).empty()};}
std::vector<ModelInfo> HarnessProvider::models()const{return {{"auto","Harness default",config_.id,{}}};}
ChatResult HarnessProvider::send(const ChatRequest& q,std::stop_token stop){
 ChatResult result;result.provider_id=config_.id;result.model_id=q.model;
 if(stop.stop_requested()){result.failure=ChatFailureKind::Cancelled;return result;}
 if(!config_.enabled||config_.executable.empty()){result.failure=ChatFailureKind::NotConfigured;return result;}
 auto executable=find_executable(config_.executable);
 if(executable.empty()){result.failure=ChatFailureKind::NotFound;return result;}
 std::error_code ec;auto workspace=std::filesystem::canonical(q.workspace,ec);
 if(ec||!std::filesystem::is_directory(workspace)){result.failure=ChatFailureKind::Other;result.error="Workspace is not an accessible directory";return result;}
 ProcessSpec spec;spec.argv={executable.string()};spec.argv.insert(spec.argv.end(),config_.argv.begin(),config_.argv.end());
 std::string prompt="Approved workspace: "+workspace.string()+"\nWork only within this root. Ask before outside-workspace, destructive, credential, or external operations.\n";
 for(const auto& m:q.history)prompt+=m.role+": "+m.content+"\n";
 prompt+="user: "+q.user_text;
 if(config_.prompt_on_stdin)spec.stdin_text=prompt;else spec.argv.push_back(prompt);
 if(!q.model.empty()&&q.model!="auto"){spec.argv.push_back(config_.model_flag);spec.argv.push_back(q.model);}
 spec.argv.insert(spec.argv.end(),config_.extra_argv.begin(),config_.extra_argv.end());
 for(const auto& a:spec.argv)if(a.find('\0')!=std::string::npos){result.error="Invalid NUL in harness argument";return result;}
 spec.cwd=workspace.string();spec.timeout=config_.timeout;spec.max_stdout=config_.max_stdout;spec.max_stderr=config_.max_stderr;spec.stop=stop;
 // Once handed to the supervisor, absence of edits cannot be established from output or exit codes.
 result.workspace_may_have_changed=true;
 ProcessResult process;
 try{process=execute_(spec);}catch(...){result.failure=ChatFailureKind::AmbiguousExecution;result.error="Harness execution failed";return result;}
 result.exit_code=process.exit_code;
 if(process.cancelled||stop.stop_requested())result.failure=ChatFailureKind::Cancelled;
 else if(process.timed_out)result.failure=ChatFailureKind::Timeout;
 else if(process.exit_code!=0||process.signal)result.failure=ChatFailureKind::AmbiguousExecution;
 else if(process.stdout_truncated)result.failure=ChatFailureKind::InvalidResponse;
 else {
  try{
   if(config_.output==HarnessOutput::Text)result.text=process.stdout_text;
   else {std::istringstream lines(process.stdout_text);std::string line;while(std::getline(lines,line)){if(line.empty())continue;auto event=Json::parse(line);if(event.has("type")&&event.at("type").str()=="turn.failed")throw std::runtime_error("failed turn");if(event.has("item")){const auto& item=event.at("item");if(item.has("type")&&item.at("type").str()=="agent_message"&&item.has("text")){if(!result.text.empty())result.text+='\n';result.text+=item.at("text").str();}}}}
   if(result.text.empty())throw std::runtime_error("empty response");
   result.ok=true;result.failure=ChatFailureKind::None;
  }catch(...){result.failure=ChatFailureKind::InvalidResponse;}
 }
 if(!result.ok)result.error="Harness failed: "+failure_name(result.failure);
 return result;
}
}
