#include "locale_conf.h"
#include <stdio.h>
#include <string.h>

static int status;

static void check(int ok, const char *what)
{
    if (!ok) {
        printf("FAIL - %s\n", what);
        status = 1;
    }
}

/* Lengths come from strlen so no case can silently read past the literal. */
static int parse(const char *text, struct leonos_locale_setting *out)
{
    return leonos_locale_parse(text, strlen(text), out, LEONOS_LOCALE_MAX);
}

static const char *value_of(const struct leonos_locale_setting *s, int n,
                            const char *name)
{
    int i;
    for (i = 0; i < n; i++) {
        if (strcmp(s[i].name, name) == 0)
            return s[i].value;
    }
    return "";
}

int main(void)
{
    struct leonos_locale_setting s[LEONOS_LOCALE_MAX];
    int n;

    n = parse("LANG=zh_CN.UTF-8\n", s);
    check(n == 1 && strcmp(value_of(s, n, "LANG"), "zh_CN.UTF-8") == 0,
          "single LANG parsed");

    n = parse("# comment\n\n  LANG=en_US.UTF-8\r\nLC_TIME=de_DE.UTF-8\n", s);
    check(n == 2 && strcmp(value_of(s, n, "LC_TIME"), "de_DE.UTF-8") == 0,
          "comment, blank line and CRLF tolerated");

    n = parse("LANG=\"zh_CN.UTF-8\"\nLC_ALL='C'\n", s);
    check(n == 2 && strcmp(value_of(s, n, "LANG"), "zh_CN.UTF-8") == 0 &&
          strcmp(value_of(s, n, "LC_ALL"), "C") == 0,
          "matching quotes stripped");

    n = parse("PATH=/usr/bin\nLANG=x/y\nLANG=ok\n", s);
    check(n == 1 && strcmp(value_of(s, n, "LANG"), "ok") == 0,
          "unknown key and slash value rejected, later line wins");

    n = parse("LANG=zh_CN.UTF-8\nLANG=\n", s);
    check(n == 0, "empty value unsets the key");

    n = parse("LANG zh_CN\nLC_FOO=bar\nLANG=ja_JP.UTF-8\n", s);
    check(n == 1 && strcmp(value_of(s, n, "LANG"), "ja_JP.UTF-8") == 0,
          "malformed lines skipped without aborting");

    n = parse("LANG=aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\n", s);
    check(n == 0, "over-long value rejected");

    n = parse("LC_CTYPE=de_DE.UTF-8\nLC_NUMERIC=de_DE.UTF-8\n"
              "LC_TIME=de_DE.UTF-8\nLC_COLLATE=de_DE.UTF-8\n"
              "LC_MONETARY=de_DE.UTF-8\nLC_MESSAGES=de_DE.UTF-8\n", s);
    check(n == 6, "every accepted category fits");

    n = parse("", s);
    check(n == 0, "empty input parsed to zero settings");

    n = parse("LANG=zh_CN.UTF-8\nLANG='broken\nlang=en\n", s);
    check(n == 1 && strcmp(value_of(s, n, "LANG"), "zh_CN.UTF-8") == 0,
          "invalid quoting and legacy key cannot override LANG");

    n = leonos_locale_parse("LANG=en\nLC_ALL=C\nLANG=zh\n", 25, s, 1);
    check(n == 1 && strcmp(value_of(s, n, "LANG"), "zh") == 0,
          "capacity limit still allows replacement");

    if (status == 0)
        printf("locale conf: keys, quotes, precedence and bounds passed\n");
    return status;
}
