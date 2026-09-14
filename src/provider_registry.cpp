#include "codane/provider_registry.hpp"
#include <algorithm>
#include <stdexcept>
namespace codane {
void ProviderRegistry::add(std::shared_ptr<ChatProvider> p){if(!p||p->info().id.empty())throw std::invalid_argument("invalid provider");if(!providers_.emplace(p->info().id,p).second)throw std::invalid_argument("duplicate provider");}
std::shared_ptr<ChatProvider> ProviderRegistry::get(const std::string& id)const{auto i=providers_.find(id);return i==providers_.end()?nullptr:i->second;}
std::vector<std::string> ProviderRegistry::auto_order()const{std::vector<std::string> ids;for(auto id:{"codex","claude","grok","gemini","pi","custom"})if(auto p=get(id);p&&p->info().configured)ids.push_back(id);for(auto&[id,p]:providers_)if(p->info().configured&&std::find(ids.begin(),ids.end(),id)==ids.end())ids.push_back(id);return ids;}
std::vector<ProviderInfo> ProviderRegistry::list()const{std::vector<ProviderInfo> out;for(auto&[_,p]:providers_)out.push_back(p->info());return out;}
}
