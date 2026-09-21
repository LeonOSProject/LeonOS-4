#ifndef LEONOS_APPS_LOCALIZED_DOC_H
#define LEONOS_APPS_LOCALIZED_DOC_H

#include <locale.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <leonos/layout.h>

/* Translate only bundled document paths; arbitrary user files stay literal. */
static inline void localized_doc_path(char *out, size_t capacity, const char *path)
{
    const char *locale = setlocale(LC_MESSAGES, NULL);
    const char *base = LEONOS_LAYOUT_LEONOS_DOC "/";
    size_t prefix = strlen(base), length = 0;
    if (locale && strncmp(path, base, prefix) == 0 && !strchr(path + prefix, '/')) {
        while (locale[length] && locale[length] != '.' && locale[length] != '@') {
            char c = locale[length];
            if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                  (c >= '0' && c <= '9') || c == '_' || c == '-')) break;
            length++;
        }
        if (length && length < 64 &&
            snprintf(out, capacity, "%s%.*s/%s", base, (int)length, locale, path + prefix) < (int)capacity &&
            access(out, R_OK) == 0) return;
    }
    snprintf(out, capacity, "%s", path);
}

#endif
