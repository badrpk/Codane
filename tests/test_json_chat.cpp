#include "codane/json.hpp"
#include <stdexcept>
#include <iostream>
using namespace codane;
int main(){std::string text="line one\n\t\"quoted\"\\end\r";if(Json::parse(Json(text).dump()).str()!=text)throw std::runtime_error("chat text round trip");if(Json::parse("\"\\u263a\"").str()!="☺")throw std::runtime_error("unicode response");for(auto s:{"\"bad\\q\"","{","[","01","\"raw\n\""}){bool bad=false;try{Json::parse(s);}catch(...){bad=true;}if(!bad)throw std::runtime_error("invalid JSON accepted");}std::cout<<"JSON chat tests passed\n";}
