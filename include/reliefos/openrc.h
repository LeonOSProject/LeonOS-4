#ifndef RELIEFOS_OPENRC_H
#define RELIEFOS_OPENRC_H
/* Returns the real command exit status, or -1 on transport/authentication error. */
int reliefos_openrc_run(const char *service, const char *action);
/* True only if OpenRC's default runlevel includes this service. */
int reliefos_openrc_enabled(const char *service);
/* On-demand UI worker; owns no service state. Returns child PID or -1.
 * poll returns 0 pending, 1 with command status, -1 on wait error.
 * Internal worker errors produce status 125. Caller must poll to reap. */
int reliefos_openrc_spawn(const char *service, const char *action);
int reliefos_openrc_poll(int child, int *result);
#endif
