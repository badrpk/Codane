#pragma once
#include "codane/provider_registry.hpp"
#include <functional>
#include <memory>
#include <map>
namespace codane {
class ProviderRouter {
 const ProviderRegistry& registry_;
public:
 explicit ProviderRouter(const ProviderRegistry& r):registry_(r){}
 using Observer=std::function<void(const std::string&,const ChatResult*)>;
 ChatResult send(const ChatRequest&,const std::string& policy="auto",std::stop_token={},bool explicit_fallback=false,Observer={}) const;
};
}
