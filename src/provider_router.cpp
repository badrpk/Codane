#include "codane/provider_router.hpp"
#include <algorithm>
namespace codane {
ChatResult ProviderRouter::send(const ChatRequest& q,const std::string& policy,std::stop_token stop,bool /*explicit_fallback*/,Observer observe)const{
 auto order=policy=="auto"?registry_.auto_order():std::vector<std::string>{policy};
 ChatResult last;last.failure=ChatFailureKind::NotConfigured;last.error="No configured provider";
 for(const auto& id:order){if(stop.stop_requested()){last.failure=ChatFailureKind::Cancelled;break;}auto p=registry_.get(id);if(observe)observe(id,nullptr);
  if(!p){last={};last.provider_id=id;last.failure=ChatFailureKind::NotConfigured;}else {try{last=p->send(q,stop);}catch(...){last={};last.provider_id=id;last.failure=ChatFailureKind::AmbiguousExecution;last.workspace_may_have_changed=true;}}
  if(observe)observe(id,&last);
  if(last.ok||last.workspace_may_have_changed)break;
  switch(last.failure){case ChatFailureKind::NotFound:case ChatFailureKind::NotConfigured:case ChatFailureKind::Auth:case ChatFailureKind::RateLimited:case ChatFailureKind::Unavailable:break;default:return last;}
 }
 return last;
}
}
