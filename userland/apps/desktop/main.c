#include "desktop.h"
#include <reliefos/pam_session.h>
#include <reliefos/environment.h>
#include <locale.h>

extern char **environ;

int main(void)
{
    char **inherited = environ;
    char **configured = NULL;
    /* The graphical VT session loads its locale for child applications. */
    unsetenv("LANG");
    unsetenv("LC_ALL");
    unsetenv("LC_MESSAGES");
    unsetenv("LC_CTYPE");
    unsetenv("LC_NUMERIC");
    unsetenv("LC_TIME");
    unsetenv("LC_COLLATE");
    unsetenv("LC_MONETARY");
    if (reliefos_environment_build(NULL, &configured) == 0) environ = configured;
    if (!setlocale(LC_ALL, "")) {
        const char *language = getenv("LANG");
        if (language) (void)setlocale(LC_ALL, language);
    }
    bindtextdomain("leonos", RELIEFOS_LAYOUT_LOCALE);
    textdomain("leonos");
    /* Keep the configured vector installed for the desktop lifetime so every
     * application it launches inherits the same locale. */
    (void)inherited;
    if (reliefos_session_initialize() < 0) {
        perror("Initialize PAM accounts");
        return 1;
    }
    reliefos_launch_use_session(1);
    desktop_run();
    return 0;
}
