#include "codane/chat_provider.hpp"
#include "codane/harness_provider.hpp"
#include "codane/provider_router.hpp"
#include "codane/provider_registry.hpp"
#include <iostream>
#include <stdexcept>
using namespace codane;
void check(bool x,const char* n){if(!x)throw std::runtime_error(n);}
int main(){
 auto c=harness_preset("codex"); c.executable="/bin/echo";
 ProcessSpec observed;
 HarnessProvider p(c,[&](const ProcessSpec& s){observed=s;ProcessResult r;r.exit_code=0;r.stdout_text="{\"type\":\"item.completed\",\"item\":{\"type\":\"agent_message\",\"text\":\"hello\"}}\n";return r;});
 ChatRequest q;q.workspace=std::filesystem::current_path();q.user_text="$(touch escaped); 'hello'";q.model="model;literal";q.history={{"user","before"},{"assistant","answer"}};
 auto r=p.send(q,{});
 check(r.ok&&r.text=="hello","Codex normalized response");
 check(observed.argv==std::vector<std::string>{"/bin/echo","exec","--json","-","--model","model;literal"},"argv literal model");
 check(observed.stdin_text.find(q.user_text)!=std::string::npos&&observed.stdin_text.find("before")!=std::string::npos,"stdin history and prompt");
 check(observed.cwd==q.workspace.string(),"workspace cwd");
 c.executable="/does/not/exist";HarnessProvider absent(c);check(absent.send(q,{}).failure==ChatFailureKind::NotFound,"missing executable");
 std::vector<std::string> attempts;ProviderRegistry registry;
 for(auto id:{"custom","gemini","claude","codex","grok","pi"}){auto cfg=harness_preset(id);cfg.enabled=true;cfg.executable="/bin/echo";registry.add(std::make_shared<HarnessProvider>(cfg,[&,id](const ProcessSpec&){attempts.push_back(id);ProcessResult x;x.exit_code=9;return x;}));}
 ProviderRouter router(registry);r=router.send(q,"auto",{},true);check(attempts==std::vector<std::string>{"codex"}&&r.workspace_may_have_changed,"stop fallback after launch");
 attempts.clear();r=router.send(q,"gemini");check(attempts==std::vector<std::string>{"gemini"},"explicit provider");
 attempts.clear();q.workspace="/missing-codane-chat-workspace";r=router.send(q,"gemini",{},true);check(attempts.empty()&&r.provider_id=="gemini","explicit selection never falls back");q.workspace=std::filesystem::current_path();
 check(registry.auto_order()==std::vector<std::string>{"codex","claude","grok","gemini","pi","custom"},"Auto order");
 check(!harness_preset("pi").enabled&&!harness_preset("custom").enabled,"optional disabled");
 ProviderRegistry safe;auto missing=harness_preset("codex");missing.executable="/does/not/exist";safe.add(std::make_shared<HarnessProvider>(missing));auto next=harness_preset("claude");next.executable="/bin/echo";safe.add(std::make_shared<HarnessProvider>(next,[](const ProcessSpec&){ProcessResult x;x.exit_code=0;x.stdout_text="safe fallback";return x;}));ProviderRouter safe_router(safe);r=safe_router.send(q,"codex",{},true);check(r.failure==ChatFailureKind::NotFound&&r.provider_id=="codex"&&!r.workspace_may_have_changed,"explicit missing provider never falls back");r=safe_router.send(q);check(r.ok&&r.provider_id=="claude","fallback before launch");
 std::cout<<"chat provider tests passed\n";
}
