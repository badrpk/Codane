#include "codane/chat_provider.hpp"
namespace codane {
std::string failure_name(ChatFailureKind k){
 switch(k){
#define CASE(x) case ChatFailureKind::x: return #x;
 CASE(None) CASE(NotFound) CASE(NotConfigured) CASE(Auth) CASE(RateLimited) CASE(Unavailable) CASE(Timeout) CASE(Cancelled) CASE(AmbiguousExecution) CASE(InvalidResponse) CASE(Other)
#undef CASE
 } return "Other";
}
}
