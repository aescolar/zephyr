#undef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#include <stddef.h>
#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <execinfo.h>
#include <errno.h>
#include <nsi_tracing.h>
#include "nsi_safe_call.h"

#define BACKTRACE_DEPTH 100

void nsi_stack_trace(unsigned int skip_bef, unsigned int skip_after){
  void* callstack[BACKTRACE_DEPTH];
  int i, frames;
  char **strs;

  nsi_print_warning(" %s (trying to recover call trace):\n", __func__);
  nsi_print_warning(" Binary/Library(Function_Name+Offset) [Absolute Address in Memory]:\n");
  frames = backtrace(callstack, BACKTRACE_DEPTH);
  strs = backtrace_symbols(callstack, frames);
  if (strs == NULL){
    raise(SIGSEGV);
  }
  for (i = 1 + skip_bef; i < frames - skip_after; ++i) { //we skip printing ourselves and as many as the caller tells us)
	  nsi_print_warning("  Called from %s\n", strs[i]);
  }
  free(strs);
  nsi_print_warning("-------------------------------------------------\n");
}

void sigaction_segfault(int signal, siginfo_t *si, void *arg)
{
	nsi_print_warning("SEGMENTATION FAULT (address %p)\n", si->si_addr);
	nsi_stack_trace(2, 2);
	raise(SIGSEGV); //Let the default handler take it from here
}

void nsi_segfault_register(void)
{
	struct sigaction sa;

	sa.sa_sigaction = sigaction_segfault;
	NSI_SAFE_CALL(sigemptyset(&sa.sa_mask)); //check for failure
	sa.sa_flags = SA_SIGINFO | SA_RESETHAND; //And reset the handler when it triggers, so another segfault does not infinite-loop, and we can generate the coredump if the OS is configured so
	NSI_SAFE_CALL(sigaction(SIGSEGV, &sa, NULL));
}
