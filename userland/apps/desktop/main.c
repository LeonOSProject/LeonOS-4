#include "desktop.h"
#include <leonos/pam_session.h>
#include <leonos/environment.h>
#include <locale.h>

extern char **environ;

int main(void)
{
    char **inherited = environ;
    char **configured = NULL;
    /* OpenRC starts the desktop directly. Load its locale without exporting
     * defaults back into the launch environment: new apps reread locale.conf. */
    /* OpenRC may export its own stale LANG. The desktop service is the locale
     * boundary, so let /etc/leonos/locale.conf seed this session. */
    unsetenv("LANG");
    unsetenv("LC_ALL");
    unsetenv("LC_MESSAGES");
    unsetenv("LC_CTYPE");
    unsetenv("LC_NUMERIC");
    unsetenv("LC_TIME");
    unsetenv("LC_COLLATE");
    unsetenv("LC_MONETARY");
    if (leonos_environment_build(NULL, &configured) == 0) environ = configured;
    if (!setlocale(LC_ALL, "")) {
        const char *language = getenv("LANG");
        if (language) (void)setlocale(LC_ALL, language);
    }
    bindtextdomain("leonos", LEONOS_LAYOUT_LOCALE);
    textdomain("leonos");
    /* Keep the configured vector installed for the desktop lifetime so every
     * application it launches inherits the same locale. */
    (void)inherited;
    if (leonos_session_initialize() < 0) {
        perror("Initialize PAM accounts");
        return 1;
    }
    leonos_launch_use_session(1);
    desktop_run();
    return 0;
}
