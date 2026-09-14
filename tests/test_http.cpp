#include "codane/http_transport.hpp"
#include <stdexcept>
#include <iostream>
using namespace codane;
void check(bool b,const char* s){if(!b)throw std::runtime_error(s);}
int main(){
 FakeHttpTransport fake; fake.responses.push_back({200,"response"});
 HttpRequest q;q.url="https://example.invalid/v1";q.body="literal $(nothing)\n\"quoted\"";
 check(fake.perform(q,{}).body=="response"&&fake.requests.size()==1,"fake transport");
 ProcessSpec seen;ProcessResult next;next.exit_code=0;next.stdout_text="body\n200";
 CurlHttpTransport curl([&](const ProcessSpec& s){seen=s;return next;});
 auto r=curl.perform(q,{});check(r.status==200&&r.body=="body","curl status framing");
 check(seen.argv==std::vector<std::string>({"curl","-q","--config","-"}),"safe curl argv");
 check(seen.stdin_text.find("data-binary = ")!=std::string::npos,"body via stdin config");
 next.exit_code=28;check(curl.perform(q,{}).failure==ChatFailureKind::Timeout,"curl timeout");
 next.exit_code=7;check(curl.perform(q,{}).failure==ChatFailureKind::Unavailable,"connection failure");
 next.exit_code=0;next.stdout_truncated=true;check(curl.perform(q,{}).failure==ChatFailureKind::InvalidResponse,"truncation");
 std::stop_source stop;stop.request_stop();check(curl.perform(q,stop.get_token()).failure==ChatFailureKind::Cancelled,"cancelled");
 q.url="http://example.invalid";check(curl.perform(q,{}).failure==ChatFailureKind::NotConfigured,"HTTPS required");
 q.url="https://example.invalid";q.headers["Authorization"]="bad\nheader";check(curl.perform(q,{}).failure==ChatFailureKind::NotConfigured,"header injection rejected");
 std::cout<<"HTTP tests passed\n";
}
