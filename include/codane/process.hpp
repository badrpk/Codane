#pragma once
#include "codane/runtime.hpp"
namespace codane {
struct ProcessSpec {std::vector<std::string> argv; std::string stdin_text; std::string cwd; std::map<std::string,std::string> environment; std::chrono::milliseconds timeout{60000}; size_t max_stdout=1<<20,max_stderr=1<<20; std::stop_token stop; std::filesystem::path cancel_file;};
struct ProcessResult {int exit_code=-1,signal=0; std::string stdout_text,stderr_text; bool stdout_truncated=false,stderr_truncated=false,cancelled=false,timed_out=false; std::chrono::milliseconds duration{0};};
ProcessResult run_process(const ProcessSpec&);
}
