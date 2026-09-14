#include "codane/chat_config.hpp"
#include <cstdlib>
#include <fstream>
#include <cmath>
namespace codane {
namespace {
std::string env(const char* name){const char* value=std::getenv(name);return value?value:"";}
std::filesystem::path home(){auto h=env("HOME");if(h.empty())throw std::runtime_error("HOME is required for chat storage");return h;}
std::vector<std::string> strings(const Json& j){std::vector<std::string> out;for(const auto& a:j.arr())out.push_back(a.str());return out;}
size_t bounded(const Json& j,size_t max){double n=j.number();if(!std::isfinite(n)||n<1||n>double(max)||std::floor(n)!=n)throw std::runtime_error("invalid harness resource limit");return size_t(n);}
void overlay(ChatConfig& c,const std::filesystem::path& file){
 if(!std::filesystem::exists(file))return;
 std::ifstream f(file);if(!f)throw std::runtime_error("Cannot read chat configuration");
 Json j;try{j=Json::parse(std::string(std::istreambuf_iterator<char>(f),{}));
 if(j.has("provider"))c.provider=j.at("provider").str();if(j.has("model"))c.model=j.at("model").str();if(j.has("fallback"))c.explicit_fallback=j.at("fallback").boolean();
 if(j.has("harnesses"))for(const auto&[id,v]:j.at("harnesses").obj()){
  auto it=c.harnesses.find(id);if(it==c.harnesses.end())throw std::runtime_error("unknown harness");auto& h=it->second;
  if(v.has("enabled"))h.enabled=v.at("enabled").boolean();if(v.has("executable"))h.executable=v.at("executable").str();
  if(v.has("argv"))h.argv=strings(v.at("argv"));if(v.has("extra_argv"))h.extra_argv=strings(v.at("extra_argv"));
  if(v.has("model_flag"))h.model_flag=v.at("model_flag").str();if(v.has("prompt_on_stdin"))h.prompt_on_stdin=v.at("prompt_on_stdin").boolean();
  if(v.has("timeout_ms"))h.timeout=std::chrono::milliseconds(bounded(v.at("timeout_ms"),86400000));
  if(v.has("max_stdout"))h.max_stdout=bounded(v.at("max_stdout"),64<<20);if(v.has("max_stderr"))h.max_stderr=bounded(v.at("max_stderr"),64<<20);
  if(v.has("output")){auto format=v.at("output").str();if(format!="text"&&format!="codex-jsonl")throw std::runtime_error("invalid format");h.output=format=="text"?HarnessOutput::Text:HarnessOutput::CodexJsonl;}
 }
 }catch(...){throw std::runtime_error("Invalid chat configuration (check provider, harness fields and resource limits)");}
}
}
std::filesystem::path chat_user_config(){auto base=env("XDG_CONFIG_HOME");return (base.empty()?home()/".config":std::filesystem::path(base))/"codane/config.json";}
std::filesystem::path chat_state_root(){auto base=env("XDG_STATE_HOME");return (base.empty()?home()/".local/state":std::filesystem::path(base))/"codane/chats";}
ChatConfig ChatConfig::load(const std::filesystem::path& workspace,const std::filesystem::path& user_file){
 ChatConfig c;for(auto id:{"codex","claude","grok","gemini","pi","custom"})c.harnesses.emplace(id,harness_preset(id));
 if(auto p=env("CODANE_PROVIDER");!p.empty())c.provider=p;if(auto m=env("CODANE_MODEL");!m.empty())c.model=m;
 overlay(c,user_file.empty()?chat_user_config():user_file);overlay(c,workspace/".codane/config.json");return c;
}
ProviderRegistry ChatConfig::registry()const{ProviderRegistry r;for(const auto&[_,h]:harnesses)r.add(std::make_shared<HarnessProvider>(h));return r;}
}
