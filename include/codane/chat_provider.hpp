#pragma once
#include <filesystem>
#include <stop_token>
#include <string>
#include <vector>
namespace codane {
enum class ProviderKind { Harness, Api };
enum class ChatFailureKind { None, NotFound, NotConfigured, Auth, RateLimited, Unavailable, Timeout, Cancelled, AmbiguousExecution, InvalidResponse, Other };
std::string failure_name(ChatFailureKind);
struct ModelInfo { std::string id, display_name, vendor; std::vector<std::string> tags; };
struct ProviderInfo { std::string id, display_name; ProviderKind kind=ProviderKind::Harness; bool configured=false, available=false; };
struct ChatMessage { std::string role, content; };
struct ChatRequest { std::filesystem::path workspace; std::vector<ChatMessage> history; std::string user_text, model; };
struct ChatResult { bool ok=false; int exit_code=-1; std::string text,error,provider_id,model_id; ChatFailureKind failure=ChatFailureKind::Other; bool workspace_may_have_changed=false; };
class ChatProvider { public: virtual ~ChatProvider()=default; virtual ProviderInfo info() const=0; virtual std::vector<ModelInfo> models() const=0; virtual ChatResult send(const ChatRequest&,std::stop_token)=0; };
}
