// SPDX-License-Identifier: GPL-2.0-or-later
#include "host_diagnostics.h"
#include <signal.h>
#include <unistd.h>
#include <ucontext.h>
#include <cstdio>
#include <initializer_list>
#include <sys/syscall.h>
namespace {
void crash(int signal,siginfo_t *info,void *context)
{
    auto *state=(ucontext_t *)context;
    char line[256];int bytes=std::snprintf(line,sizeof line,
        "host-crash signal=%d address=%p pc=0x%llx sp=0x%llx tid=%ld\n",
        signal,info->si_addr,(unsigned long long)state->uc_mcontext.pc,
        (unsigned long long)state->uc_mcontext.sp,syscall(SYS_gettid));
    if(bytes>0)write(STDERR_FILENO,line,(size_t)bytes);
    struct sigaction action{};action.sa_handler=SIG_DFL;sigemptyset(&action.sa_mask);
    sigaction(signal,&action,nullptr);
    syscall(SYS_tgkill,getpid(),syscall(SYS_gettid),signal);
}
}
extern "C" void recomp_install_host_diagnostics(void)
{
    struct sigaction action{};action.sa_sigaction=crash;sigemptyset(&action.sa_mask);action.sa_flags=SA_SIGINFO;
    for(int signal:{SIGSEGV,SIGBUS,SIGILL,SIGABRT,SIGFPE})sigaction(signal,&action,nullptr);
}
