#include "codane/process.hpp"
#include <sys/wait.h>
#include <poll.h>
#include <signal.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include <cerrno>
#include <algorithm>
namespace codane {
namespace {
void close_fd(int& fd){if(fd>=0){close(fd);fd=-1;}}
struct Pipes {int in[2]{-1,-1},out[2]{-1,-1},err[2]{-1,-1};~Pipes(){for(auto a:{in,out,err})for(int i=0;i<2;++i)close_fd(a[i]);}};
struct BlockPipe {
 sigset_t set,old; bool pending=false;
 BlockPipe(){sigemptyset(&set);sigaddset(&set,SIGPIPE);sigset_t p;sigpending(&p);pending=sigismember(&p,SIGPIPE);pthread_sigmask(SIG_BLOCK,&set,&old);}
 ~BlockPipe(){if(!pending){timespec t{};while(sigtimedwait(&set,nullptr,&t)>=0){}}pthread_sigmask(SIG_SETMASK,&old,nullptr);}
};
}
ProcessResult run_process(const ProcessSpec& s){
 if(s.argv.empty())throw std::invalid_argument("empty argv");
 for(const auto& a:s.argv)if(a.find('\0')!=std::string::npos)throw std::invalid_argument("NUL in argv");
 ProcessResult r;if(s.stop.stop_requested()){r.cancelled=true;return r;}
 auto started=Clock::now();Pipes pipes;
 if(pipe(pipes.in)||pipe(pipes.out)||pipe(pipes.err))throw std::system_error(errno,std::generic_category());
 for(auto a:{pipes.in,pipes.out,pipes.err})for(int i=0;i<2;++i)fcntl(a[i],F_SETFD,FD_CLOEXEC);
 std::vector<char*> argv;for(const auto& a:s.argv)argv.push_back(const_cast<char*>(a.c_str()));argv.push_back(nullptr);
 pid_t pid=fork();
 if(pid<0)throw std::system_error(errno,std::generic_category());
 if(pid==0){
  setpgid(0,0);dup2(pipes.in[0],0);dup2(pipes.out[1],1);dup2(pipes.err[1],2);
  for(auto a:{pipes.in,pipes.out,pipes.err})for(int i=0;i<2;++i)close(a[i]);
  if(!s.cwd.empty()&&chdir(s.cwd.c_str())!=0)_exit(126);
  for(const auto&[k,v]:s.environment)if(setenv(k.c_str(),v.c_str(),1)!=0)_exit(126);
  execvp(argv[0],argv.data());_exit(127);
 }
 setpgid(pid,pid);close_fd(pipes.in[0]);close_fd(pipes.out[1]);close_fd(pipes.err[1]);
 for(int fd:{pipes.in[1],pipes.out[0],pipes.err[0]})fcntl(fd,F_SETFL,fcntl(fd,F_GETFL,0)|O_NONBLOCK);
 BlockPipe block;size_t sent=0;bool term=false,reaped=false;int status=0;auto term_at=started,exit_at=started;
 auto capture=[&](int& fd,std::string& text,size_t limit,bool& truncated){char buf[8192];for(int reads=0;fd>=0&&reads<32;++reads){ssize_t n=read(fd,buf,sizeof buf);if(n>0){size_t keep=std::min(size_t(n),limit-text.size());text.append(buf,keep);if(keep<size_t(n))truncated=true;}else if(n==0){close_fd(fd);break;}else if(errno==EINTR)continue;else if(errno!=EAGAIN&&errno!=EWOULDBLOCK){close_fd(fd);break;}else break;}};
 while(!reaped||pipes.out[0]>=0||pipes.err[0]>=0){
  auto now=Clock::now();std::error_code ec;
  bool cancelled=s.stop.stop_requested()||(!s.cancel_file.empty()&&std::filesystem::exists(s.cancel_file,ec));
  bool timed=s.timeout.count()>0&&now-started>=s.timeout;
  if(!term&&(cancelled||timed)){r.cancelled=cancelled;r.timed_out=!cancelled&&timed;kill(-pid,SIGTERM);term=true;term_at=now;}
  if(term&&now-term_at>=std::chrono::milliseconds(1000))kill(-pid,SIGKILL);
  if(reaped&&now-exit_at>=std::chrono::milliseconds(1000)){kill(-pid,SIGKILL);close_fd(pipes.out[0]);close_fd(pipes.err[0]);break;}
  if(sent==s.stdin_text.size())close_fd(pipes.in[1]);
  pollfd fds[]={{pipes.in[1],POLLOUT,0},{pipes.out[0],POLLIN,0},{pipes.err[0],POLLIN,0}};
  poll(fds,3,20);
  if(pipes.in[1]>=0&&fds[0].revents){ssize_t n=write(pipes.in[1],s.stdin_text.data()+sent,s.stdin_text.size()-sent);if(n>0)sent+=size_t(n);else if(n<0&&errno!=EINTR&&errno!=EAGAIN)close_fd(pipes.in[1]);}
  capture(pipes.out[0],r.stdout_text,s.max_stdout,r.stdout_truncated);capture(pipes.err[0],r.stderr_text,s.max_stderr,r.stderr_truncated);
  if(!reaped){pid_t w=waitpid(pid,&status,WNOHANG);if(w==pid){reaped=true;exit_at=Clock::now();close_fd(pipes.in[1]);}else if(w<0&&errno!=EINTR)throw std::system_error(errno,std::generic_category());}
 }
 if(WIFEXITED(status))r.exit_code=WEXITSTATUS(status);
 if(WIFSIGNALED(status))r.signal=WTERMSIG(status);
 r.duration=std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now()-started);return r;
}
}
