#ifndef RELIEFOS_PAM_SESSION_H
#define RELIEFOS_PAM_SESSION_H
#include <reliefos/auth.h>
int reliefos_session_initialize(void);
int reliefos_session_current(struct reliefos_user_info *user);
int reliefos_session_apply(void);
int reliefos_pam_session_wait(void);
int reliefos_pam_login(const char *name, char *password, struct reliefos_user_info *user);
#endif
