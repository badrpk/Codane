#include "codane/runtime.hpp"
namespace codane {std::string state_name(NodeState s){static const char*n[]={"pending","ready","running","succeeded","failed","blocked","cancelled","skipped"};return n[(int)s];}NodeState parse_state(const std::string&s){for(int i=0;i<8;i++)if(s==state_name((NodeState)i))return(NodeState)i;throw std::invalid_argument("unknown node state");}}
