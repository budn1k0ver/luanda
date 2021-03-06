#include <luanda.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/ptrace.h>
#include <sys/wait.h>
#include <sys/user.h>

static const pid_t ignored_pid;
static const void *ignored_ptr;
static const int no_continue_signal = 0;

static void setup_inferior(const char *path, char *const argv[]) {
  ptrace(PTRACE_TRACEME, ignored_pid, ignored_ptr, no_continue_signal);
  if (execv(path, argv) < 0) {
    perror("execv error");
  }
}

static void attach_to_inferior(pid_t pid) {
    int status;
    waitpid(pid, &status, 0);

    if(WIFSTOPPED(status) && WSTOPSIG(status) == SIGTRAP) {
        return;
    } else {
        perror("ABORT");
        fprintf(stderr, "Unexpected status for inferior %d when attaching\n", pid);
        abort();
    }
}

luanda_inferior_t luanda_inferior_exec(const char *path, char *const argv[]) {
  pid_t result;
  
  do {
    result = fork();

    switch (result)
    {
    case 0:
      setup_inferior(path, argv);
      break;
    case -1:
      break;
    default:
      attach_to_inferior(result);
      break;
    }
  } while(result == -1 && errno == EAGAIN);
  return result;
}

void luanda_inferior_continue(luanda_inferior_t inferior) 
{
    pid_t pid = inferior;
    ptrace(PTRACE_CONT, pid, ignored_ptr, no_continue_signal);

    while(1)
    {
        int status;
        waitpid(pid, &status, 0);
        
        fprintf(stderr, "STATUS 1 %d\n", status);
        if (WIFSTOPPED(status) && WSTOPSIG(status) == SIGTRAP) {
            struct user_regs_struct regs;

            luanda_breakpoint_t bp = breakpoint_resolve(inferior);
            breakpoint_remove(inferior, bp);
            breakpoint_trigger_callback(inferior, bp);
            
            ptrace(PTRACE_GETREGS, pid, ignored_ptr, &regs);
            regs.rip -= 1;
            ptrace(PTRACE_SETREGS, pid, ignored_ptr, &regs);

            ptrace(PTRACE_CONT, pid, ignored_ptr, no_continue_signal);
        } else if (WIFEXITED(status)) {
            return;
        }
    } 
}
