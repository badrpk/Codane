#pragma once
#include "codane/chat_provider.hpp"
#include <memory>
#include <map>
namespace codane {
class ProviderRegistry {
 std::map<std::string,std::shared_ptr<ChatProvider>> providers_;
public:
 void add(std::shared_ptr<ChatProvider>);
 std::shared_ptr<ChatProvider> get(const std::string&) const;
 std::vector<ProviderInfo> list() const;
 std::vector<std::string> auto_order() const;
};
}
