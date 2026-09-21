#include "locale_conf.h"
#include <string.h>

static const char *const locale_known[] = {
    "LANG", "LC_ALL", "LC_CTYPE", "LC_NUMERIC", "LC_TIME",
    "LC_COLLATE", "LC_MONETARY", "LC_MESSAGES"
};

#define LOCALE_KNOWN_COUNT ((int)(sizeof locale_known / sizeof *locale_known))

static int locale_slot(const struct leonos_locale_setting *out, int count,
                       const char *name, size_t name_len)
{
    int i;
    for (i = 0; i < count; i++) {
        if (strlen(out[i].name) == name_len &&
            memcmp(out[i].name, name, name_len) == 0)
            return i;
    }
    return -1;
}

static int locale_known_index(const char *name, size_t name_len)
{
    int i;
    for (i = 0; i < LOCALE_KNOWN_COUNT; i++) {
        if (strlen(locale_known[i]) == name_len &&
            memcmp(locale_known[i], name, name_len) == 0)
            return i;
    }
    return -1;
}

static size_t locale_span_ws(const char *t, size_t length, size_t pos)
{
    while (pos < length && (t[pos] == ' ' || t[pos] == '\t'))
        pos++;
    return pos;
}

/* Drop trailing CR/blanks so CRLF files parse identically to LF ones. */
static size_t locale_trim_end(const char *t, size_t start, size_t end)
{
    while (end > start && (t[end - 1U] == '\r' || t[end - 1U] == ' ' ||
                           t[end - 1U] == '\t'))
        end--;
    return end;
}

static int locale_value_clean(const char *value, size_t length)
{
    size_t i;
    if (length == 0 || length + 1U > LEONOS_LOCALE_VALUE_LEN)
        return 0;
    for (i = 0; i < length; i++) {
        unsigned char c = (unsigned char)value[i];
        if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
              (c >= '0' && c <= '9') || c == '.' || c == '_' ||
              c == '@' || c == '-'))
            return 0;
    }
    return 1;
}

int leonos_locale_parse(const char *text, size_t length,
                        struct leonos_locale_setting *out, int capacity)
{
    size_t pos = 0;
    int count = 0;
    if (!text || !out || capacity <= 0)
        return 0;
    while (pos < length) {
        size_t end = pos, key, key_len, val, val_len;
        int slot;
        while (end < length && text[end] != '\n')
            end++;
        key = locale_span_ws(text, end, pos);
        if (key >= end || text[key] == '#') {
            pos = end + 1U;
            continue;
        }
        {
            size_t p = key;
            while (p < end && ((text[p] >= 'A' && text[p] <= 'Z') ||
                               (text[p] >= 'a' && text[p] <= 'z') ||
                               text[p] == '_'))
                p++;
            key_len = p - key;
            if (key_len == 0 || p >= end || text[p] != '=') {
                pos = end + 1U;
                continue;
            }
            val = locale_span_ws(text, end, p + 1U);
        }
        val_len = locale_trim_end(text, val, end) - val;
        if (val_len >= 2U &&
            ((text[val] == '"' && text[val + val_len - 1U] == '"') ||
             (text[val] == '\'' && text[val + val_len - 1U] == '\''))) {
            val++;
            val_len -= 2U;
        }
        if (val_len == 0U) { /* empty value: unset, drop the key entirely */
            int i = locale_slot(out, count, text + key, key_len);
            if (i >= 0) {
                count--;
                while (i < count) {
                    out[i] = out[i + 1];
                    i++;
                }
            }
            pos = end + 1U;
            continue;
        }
        if (locale_known_index(text + key, key_len) < 0 ||
            !locale_value_clean(text + val, val_len)) {
            pos = end + 1U;
            continue;
        }
        slot = locale_slot(out, count, text + key, key_len);
        if (slot < 0) {
            if (count >= capacity) {
                pos = end + 1U;
                continue;
            }
            slot = count++;
            memcpy(out[slot].name, text + key, key_len);
            out[slot].name[key_len] = '\0';
        }
        memcpy(out[slot].value, text + val, val_len);
        out[slot].value[val_len] = '\0';
        pos = end + 1U;
    }
    return count;
}
