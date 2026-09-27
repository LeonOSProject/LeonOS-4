/* Transitional compatibility forwarder (ReliefOS / ReliefNT rename).
 * Canonical declarations: <reliefos/openrc.h>.
 * No second layout definition exists behind these names. */
#ifndef LEONOS_OPENRC_H
#define LEONOS_OPENRC_H
#include <reliefos/openrc.h>

/* Old names alias the single canonical declaration. */
#define leonos_openrc_enabled reliefos_openrc_enabled
#define leonos_openrc_poll reliefos_openrc_poll
#define leonos_openrc_run reliefos_openrc_run
#define leonos_openrc_spawn reliefos_openrc_spawn
#endif /* LEONOS_OPENRC_H */
