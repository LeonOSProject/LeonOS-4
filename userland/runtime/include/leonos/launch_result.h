/* Transitional compatibility forwarder (ReliefOS / ReliefNT rename).
 * Canonical declarations: <reliefos/launch_result.h>.
 * No second layout definition exists behind these names. */
#ifndef LEONOS_LAUNCH_RESULT_H
#define LEONOS_LAUNCH_RESULT_H
#include <reliefos/launch_result.h>

/* Old names alias the single canonical declaration. */
#define leonos_launch_errno reliefos_launch_errno
#define leonos_launch_error_kind reliefos_launch_error_kind
#define leonos_launch_error_text reliefos_launch_error_text
#define leonos_launch_is_error reliefos_launch_is_error
#endif /* LEONOS_LAUNCH_RESULT_H */
