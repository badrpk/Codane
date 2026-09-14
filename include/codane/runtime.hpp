#pragma once
#include "codane/json.hpp"
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <filesystem>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>
namespace codane {
using RunId=std::string; using NodeId=std::string; using Clock=std::chrono::steady_clock;
enum class NodeState {Pending,Ready,Running,Succeeded,Failed,Blocked,Cancelled,Skipped};
std::string state_name(NodeState); NodeState parse_state(const std::string&);
struct RetryPolicy {int max_attempts=1; std::chrono::milliseconds backoff{0}; double multiplier=2;};
struct Limits {size_t max_parallel=1,max_graph_nodes=1000,max_graph_depth=100,max_stdout=1<<20,max_stderr=1<<20; std::chrono::milliseconds process_timeout{60000},graph_timeout{0};};
struct Cancellation {std::stop_source source; bool cancelled()const{return source.stop_requested();} void cancel(){source.request_stop();}};
struct ArtifactInput { std::string producer, path, resolved_path; };
struct AgentRequest {NodeId id; std::string name,prompt,workspace,model; int attempt=1; std::chrono::milliseconds timeout{60000}; std::map<std::string,std::string> environment,metadata; std::vector<ArtifactInput> artifacts;};
struct AgentResult {int attempts=0; NodeState status=NodeState::Failed; int exit_code=-1,signal=0; std::string stdout_text,stderr_text; std::chrono::milliseconds duration{0}; bool stdout_truncated=false,stderr_truncated=false; bool verifier_ran=false; std::string verifier_error;};
}
