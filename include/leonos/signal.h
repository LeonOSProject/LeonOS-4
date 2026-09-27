/* Transitional compatibility forwarder (ReliefOS / ReliefNT rename).
 * Canonical declarations: <reliefos/signal.h>.
 * No second layout definition exists behind these names. */
#ifndef LEONOS_SIGNAL_H
#define LEONOS_SIGNAL_H
#include <reliefos/signal.h>

/* Old names alias the single canonical declaration. */
#define LEONOS_SIGNAL_ACTION_GET RELIEFOS_SIGNAL_ACTION_GET
#define LEONOS_SIGNAL_ACTION_SET RELIEFOS_SIGNAL_ACTION_SET
#define LEONOS_SIGNAL_DISPOSITION_DEFAULT RELIEFOS_SIGNAL_DISPOSITION_DEFAULT
#define LEONOS_SIGNAL_DISPOSITION_IGNORE RELIEFOS_SIGNAL_DISPOSITION_IGNORE
#define LEONOS_UAPI_SIGNAL_ABI_H RELIEFOS_UAPI_SIGNAL_ABI_H
#define leonos_linux_sigaction reliefos_linux_sigaction
#define leonos_rt_sigreturn_trampoline reliefos_rt_sigreturn_trampoline
#endif /* LEONOS_SIGNAL_H */
