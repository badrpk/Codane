#pragma once
#include "codane/json.hpp"
#include "codane/runtime.hpp"

namespace codane { struct Event { uint64_t sequence; std::string timestamp,run_id,event_type,node_id; Json payload; Json json() const; }; class Journal { std::filesystem::path path; uint64_t next=1; public: explicit Journal(std::filesystem::path); void append(const std::string&,const RunId&,const std::string&,const Json& payload=Json(),const std::string& node_id=""); std::vector<Event> replay() const; uint64_t last_sequence() const{return next-1;} }; void atomic_write(const std::filesystem::path&,const std::string&); }
