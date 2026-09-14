#pragma once
#include "codane/agent.hpp"
namespace codane { struct LoopPolicy {int max_iterations=1,max_consecutive_failures=1,stagnation_limit=0;std::chrono::milliseconds max_wall_time{0};RetryPolicy retry;}; struct LoopResult {int iterations=0;bool accepted=false;}; LoopResult run_loop(AgentProvider&,const AgentRequest&,const LoopPolicy&,Cancellation&,const std::function<bool(const AgentResult&,int)>&); }
