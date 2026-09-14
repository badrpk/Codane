#include "codane/json.hpp"
#include <cctype>
#include <cmath>
#include <iomanip>
#include <sstream>
namespace codane {
namespace {
std::string esc(const std::string& s){std::string out="\"";const char* hex="0123456789abcdef";for(unsigned char c:s){switch(c){case '"':out+="\\\"";break;case '\\':out+="\\\\";break;case '\n':out+="\\n";break;case '\r':out+="\\r";break;case '\t':out+="\\t";break;default:if(c<32){out+="\\u00";out+=hex[c>>4];out+=hex[c&15];}else out+=char(c);}}return out+'"';}
struct Parser {
 const std::string& s;size_t i=0;unsigned depth=0;
 [[noreturn]] void fail(){throw std::runtime_error("invalid JSON");}
 void ws(){while(i<s.size()&&(s[i]==' '||s[i]=='\n'||s[i]=='\r'||s[i]=='\t'))++i;}
 char get(){if(i>=s.size())fail();return s[i++];}
 bool take(char c){if(i<s.size()&&s[i]==c){++i;return true;}return false;}
 unsigned hex(){unsigned n=0;for(int k=0;k<4;++k){char c=get();n*=16;if(c>='0'&&c<='9')n+=c-'0';else if(c>='a'&&c<='f')n+=c-'a'+10;else if(c>='A'&&c<='F')n+=c-'A'+10;else fail();}return n;}
 void utf8(std::string& out,unsigned cp){if(cp<=0x7f)out+=char(cp);else if(cp<=0x7ff){out+=char(0xc0|(cp>>6));out+=char(0x80|(cp&63));}else if(cp<=0xffff){out+=char(0xe0|(cp>>12));out+=char(0x80|((cp>>6)&63));out+=char(0x80|(cp&63));}else{out+=char(0xf0|(cp>>18));out+=char(0x80|((cp>>12)&63));out+=char(0x80|((cp>>6)&63));out+=char(0x80|(cp&63));}}
 std::string string(){if(get()!='"')fail();std::string out;for(;;){unsigned char c=get();if(c=='"')return out;if(c<32)fail();if(c!='\\'){out+=char(c);continue;}switch(get()){case '"':out+='"';break;case '\\':out+='\\';break;case '/':out+='/';break;case 'b':out+='\b';break;case 'f':out+='\f';break;case 'n':out+='\n';break;case 'r':out+='\r';break;case 't':out+='\t';break;case 'u':{unsigned cp=hex();if(cp>=0xd800&&cp<=0xdbff){if(get()!='\\'||get()!='u')fail();unsigned low=hex();if(low<0xdc00||low>0xdfff)fail();cp=0x10000+((cp-0xd800)<<10)+(low-0xdc00);}else if(cp>=0xdc00&&cp<=0xdfff)fail();utf8(out,cp);break;}default:fail();}}}
 Json value(){ws();if(++depth>128)fail();Json result;char c=i<s.size()?s[i]:0;
 if(c=='"')result=string();
 else if(take('{')){Json::object o;ws();if(!take('}'))for(;;){ws();auto key=string();ws();if(!take(':'))fail();auto val=value();if(!o.emplace(key,std::move(val)).second)fail();ws();if(take('}'))break;if(!take(','))fail();}result=o;}
 else if(take('[')){Json::array a;ws();if(!take(']'))for(;;){a.push_back(value());ws();if(take(']'))break;if(!take(','))fail();}result=a;}
 else if(s.compare(i,4,"true")==0){i+=4;result=true;}else if(s.compare(i,5,"false")==0){i+=5;result=false;}else if(s.compare(i,4,"null")==0){i+=4;}
 else{size_t start=i;take('-');if(!take('0')){if(i>=s.size()||s[i]<'1'||s[i]>'9')fail();while(i<s.size()&&std::isdigit((unsigned char)s[i]))++i;}if(take('.')){size_t b=i;while(i<s.size()&&std::isdigit((unsigned char)s[i]))++i;if(b==i)fail();}if(take('e')||take('E')){if(!take('+'))take('-');size_t b=i;while(i<s.size()&&std::isdigit((unsigned char)s[i]))++i;if(b==i)fail();}double n=std::stod(s.substr(start,i-start));if(!std::isfinite(n))fail();result=n;}
 --depth;return result;
 }
};
}
std::string Json::dump()const{return std::visit([](const auto& x)->std::string{using T=std::decay_t<decltype(x)>;if constexpr(std::is_same_v<T,std::nullptr_t>)return "null";else if constexpr(std::is_same_v<T,bool>)return x?"true":"false";else if constexpr(std::is_same_v<T,double>){if(!std::isfinite(x))throw std::runtime_error("non-finite JSON number");std::ostringstream o;o<<std::setprecision(17)<<x;return o.str();}else if constexpr(std::is_same_v<T,std::string>)return esc(x);else if constexpr(std::is_same_v<T,array>){std::string o="[";for(const auto& v:x){if(o.size()>1)o+=',';o+=v.dump();}return o+"]";}else{std::string o="{";for(const auto&[k,v]:x){if(o.size()>1)o+=',';o+=esc(k)+":"+v.dump();}return o+"}";}},v);}
Json Json::parse(const std::string& s){Parser p{s};auto j=p.value();p.ws();if(p.i!=s.size())throw std::runtime_error("trailing JSON");return j;}
}
