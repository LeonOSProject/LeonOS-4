#ifndef LEONOS_PTY_H
#define LEONOS_PTY_H

/*
 * PTY userland header is a thin alias layer. The wire ABI moved to the kernel
 * UAPI (<leonos/pty_abi.h>); this header re-exports it so existing
 * `#include <leonos/pty.h>` callers keep working.
 */
#include <leonos/pty_abi.h>

#endif
