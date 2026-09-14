#pragma once
#include "codane/chat_provider.hpp"
#include "codane/process.hpp"
#include <functional>
namespace codane {
enum class HarnessOutput { Text, CodexJsonl };
struct HarnessConfig {
 std::string id,display_name,executable,model_flag="--model";
 std::vector<std::string> argv,extra_argv;
 bool enabled=true, prompt_on_stdin=true;
 HarnessOutput output=HarnessOutput::Text;
 std::chrono::milliseconds timeout{120000};
 size_t max_stdout=1<<20,max_stderr=1<<20;
};
HarnessConfig harness_preset(const std::string&);
std::filesystem::path find_executable(const std::string&);
class HarnessProvider final: public ChatProvider {
 HarnessConfig config_; std::function<ProcessResult(const ProcessSpec&)> execute_;
public:
 explicit HarnessProvider(HarnessConfig,std::function<ProcessResult(const ProcessSpec&)> execute=run_process);
 ProviderInfo info() const override;
 std::vector<ModelInfo> models() const override;
 ChatResult send(const ChatRequest&,std::stop_token) override;
};
}
