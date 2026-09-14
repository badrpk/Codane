#pragma once
#include "codane/runtime.hpp"
#include "codane/verifier.hpp"
namespace codane {
class AgentProvider {public: virtual ~AgentProvider()=default; virtual AgentResult run(const AgentRequest&,std::stop_token)=0;};
class FakeProvider final: public AgentProvider {public: std::function<AgentResult(const AgentRequest&)> fn; std::atomic<int> active{0},peak{0}; AgentResult run(const AgentRequest&,std::stop_token) override;};
AgentResult run_with_retry(AgentProvider&,const AgentRequest&,const RetryPolicy&,Cancellation&,const std::function<void(int)>& on_attempt={});
AgentResult run_verified(AgentProvider&,const AgentRequest&,const RetryPolicy&,Cancellation&,const std::function<std::unique_ptr<Verifier>()>&);
}
