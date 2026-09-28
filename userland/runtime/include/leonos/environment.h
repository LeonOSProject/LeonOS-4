/* Transitional compatibility forwarder (ReliefOS / ReliefNT rename).
 * Canonical declarations: <reliefos/environment.h>.
 * No second layout definition exists behind these names. */
#ifndef LEONOS_ENVIRONMENT_H
#define LEONOS_ENVIRONMENT_H
#include <reliefos/environment.h>

/* Old names alias the single canonical declaration. */
#define LEONOS_ENV_MAX_ENTRIES RELIEFOS_ENV_MAX_ENTRIES
#define LEONOS_ENV_MAX_ENTRY_LEN RELIEFOS_ENV_MAX_ENTRY_LEN
#define LEONOS_ENV_MAX_FILE_BYTES RELIEFOS_ENV_MAX_FILE_BYTES
#define LEONOS_ENV_SCOPE_GLOBAL RELIEFOS_ENV_SCOPE_GLOBAL
#define LEONOS_ENV_SCOPE_USER RELIEFOS_ENV_SCOPE_USER
#define leonos_environment_build reliefos_environment_build
#define leonos_environment_free reliefos_environment_free
#define leonos_environment_set reliefos_environment_set
#define leonos_environment_unset reliefos_environment_unset
#endif /* LEONOS_ENVIRONMENT_H */
