#ifndef RELIEFOS_PTY_H
#define RELIEFOS_PTY_H

/*
 * PTY userland header is a thin alias layer. The wire ABI moved to the kernel
 * UAPI (<reliefos/pty_abi.h>); this header re-exports it so existing
 * `#include <reliefos/pty.h>` callers keep working.
 */
#include <reliefos/pty_abi.h>

#endif
