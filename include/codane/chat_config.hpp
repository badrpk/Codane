#pragma once
#include "codane/harness_provider.hpp"
#include "codane/provider_router.hpp"
namespace codane {
struct ChatConfig {
 std::string provider="auto",model="auto";
 bool explicit_fallback=false;
 std::map<std::string,HarnessConfig> harnesses;
 static ChatConfig load(const std::filesystem::path& workspace,const std::filesystem::path& user_file={});
 ProviderRegistry registry() const;
};
std::filesystem::path chat_state_root();
std::filesystem::path chat_user_config();
}
