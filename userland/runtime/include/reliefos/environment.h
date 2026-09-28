#ifndef RELIEFOS_ENVIRONMENT_H
#define RELIEFOS_ENVIRONMENT_H

#include <stdint.h>

#define RELIEFOS_ENV_SCOPE_GLOBAL 1U
#define RELIEFOS_ENV_SCOPE_USER 2U

#define RELIEFOS_ENV_MAX_ENTRIES 64U
#define RELIEFOS_ENV_MAX_ENTRY_LEN 256U
#define RELIEFOS_ENV_MAX_FILE_BYTES 8192U

/* Build a NULL-terminated environment for a newly spawned process.
 * Values from the current process are used as a base, then the optional
 * overrides are applied last. The returned vector must be released with
 * reliefos_environment_free(). */
int reliefos_environment_build(char *const overrides[], char ***out_envp);
void reliefos_environment_free(char **envp);

/* Update the persistent global or current-user environment file. Global
 * updates require an administrator session; user updates require a logged-in
 * account with a home directory. */
int reliefos_environment_set(uint32_t scope, const char *name, const char *value);
int reliefos_environment_unset(uint32_t scope, const char *name);

#endif
