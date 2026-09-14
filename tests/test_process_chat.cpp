#include "codane/process.hpp"
#include <iostream>
#include <stdexcept>
using namespace codane;
int main(){ProcessSpec s;s.argv={"/bin/cat"};s.stdin_text=std::string(200000,'x');s.timeout=std::chrono::seconds(5);auto r=run_process(s);if(r.stdout_text!=s.stdin_text||r.exit_code!=0)throw std::runtime_error("complete large output");s.argv={"/bin/pwd"};s.cwd="/missing-codane-workspace";s.stdin_text="";r=run_process(s);if(r.exit_code==0)throw std::runtime_error("invalid cwd must not execute");std::cout<<"process chat tests passed\n";}
