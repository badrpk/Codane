#include "codane/chat_config.hpp"
#include <fstream>
#include <stdexcept>
#include <iostream>
using namespace codane;
int main(){auto root=std::filesystem::current_path()/"config-fixture";std::filesystem::create_directories(root/"workspace/.codane");std::filesystem::create_directories(root/"user");std::ofstream(root/"user/config.json")<<R"({"provider":"claude","model":"user-model","harnesses":{"custom":{"enabled":true,"executable":"/bin/cat","extra_argv":["literal;arg"]}}})";std::ofstream(root/"workspace/.codane/config.json")<<R"({"provider":"custom","model":"workspace-model"})";auto config=ChatConfig::load(root/"workspace",root/"user/config.json");if(config.provider!="custom"||config.model!="workspace-model"||!config.harnesses.at("custom").enabled||config.harnesses.at("custom").extra_argv[0]!="literal;arg")throw std::runtime_error("config precedence and harness args");std::filesystem::remove_all(root);std::cout<<"config tests passed\n";}
