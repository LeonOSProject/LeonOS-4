#ifndef LEONOS_SIGNAL_H
#define LEONOS_SIGNAL_H

/*
 * Signal ABI re-export plus the libc-owned return trampoline. The wire
 * aliases live in the kernel UAPI (<leonos/signal_abi.h>); this header keeps
 * the userland symbol declared next to them for existing callers.
 */
#include <leonos/signal_abi.h>

/* Defined in userland/runtime/src/syscall.S. The kernel never names the symbol:
 * it records the address a signal setup passes as the handler restorer. */
void leonos_rt_sigreturn_trampoline(void);

#endif
