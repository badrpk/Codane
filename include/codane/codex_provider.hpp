#pragma once
#include "codane/agent.hpp"
#include "codane/process.hpp"
namespace codane { struct CodexConfig {std::string executable="codex",cwd,model; std::vector<std::string> extra_argv; std::chrono::milliseconds timeout{60000}; size_t max_stdout=1<<20,max_stderr=1<<20;}; class CodexProvider final:public AgentProvider {CodexConfig c; public: explicit CodexProvider(CodexConfig x={}):c(std::move(x)){} AgentResult run(const AgentRequest&,std::stop_token) override;}; }
