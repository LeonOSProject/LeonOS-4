/*
 * leonos-config - turn a Kconfig .config into the derived build inputs.
 *
 * Kconfig stays the authority on what a symbol means; this tool carries no
 * second copy of the defaults. It only re-expresses the resolved .config as C
 * headers and a Make include (plan section 7).
 *
 * Every output is rendered in full before anything is published, so a malformed
 * line cannot leave a half-written configuration behind.
 */

/* vasprintf() is a GNU extension; declaring it here keeps -std=c11 honest. */
#define _GNU_SOURCE 1

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "tools/host/common/buffer.h"
#include "tools/host/common/io.h"

#define TOOL_NAME "leonos-config"
#define KEY_PREFIX "CONFIG_"
#define KEY_PREFIX_SIZE 7u
#define NOT_SET_TEXT " is not set"

enum value_kind { KIND_BOOL, KIND_NUMBER, KIND_STRING, KIND_INVALID };

struct symbol {
    char *key;
    char *value;
    enum value_kind kind;
};

struct options {
    const char *input;
    const char *out_header;
    const char *out_installer_header;
    const char *out_make;
    const char *guard;
    const char *installer_guard;
    const char *require_license;
    const char *installer_require_license;
};

#if defined(__GNUC__) || defined(__clang__)
static int report(const char *format, ...) __attribute__((format(printf, 1, 2)));
#endif
static int report(const char *format, ...)
{
    va_list arguments;

    fprintf(stderr, "%s: ", TOOL_NAME);
    va_start(arguments, format);
    (void)vfprintf(stderr, format, arguments);
    va_end(arguments);
    fputc('\n', stderr);
    return 1;
}

static int append(struct byte_buffer *buffer, const void *data, size_t size)
{
    if (size > SIZE_MAX - buffer->len) {
        errno = EOVERFLOW;
        return -1;
    }
    if (buffer_reserve(buffer, buffer->len + size) != 0) {
        return -1;
    }
    memcpy(buffer->data + buffer->len, data, size);
    buffer->len += size;
    return 0;
}

static int append_text(struct byte_buffer *buffer, const char *text)
{
    return append(buffer, text, strlen(text));
}

#if defined(__GNUC__) || defined(__clang__)
static int appendf(struct byte_buffer *buffer, const char *format, ...) __attribute__((format(printf, 2, 3)));
#endif
static int appendf(struct byte_buffer *buffer, const char *format, ...)
{
    va_list arguments;
    char *line = NULL;
    int needed;
    int status;

    va_start(arguments, format);
    needed = vasprintf(&line, format, arguments);
    va_end(arguments);
    if (needed < 0 || line == NULL) {
        errno = ENOMEM;
        return -1;
    }
    /* vasprintf() reports the length it would have written, so a truncated
     * render is an error here rather than silently shortened output. */
    if ((size_t)needed != strlen(line)) {
        free(line);
        errno = EOVERFLOW;
        return -1;
    }
    status = append(buffer, line, (size_t)needed);
    free(line);
    return status;
}

static void usage(void)
{
    printf(
        "usage: %s --input CONFIG [output options]\n"
        "\n"
        "  --input PATH                 .config to read\n"
        "  --out-header PATH            C header for the shipped-image policy\n"
        "  --out-installer-header PATH  C header for the installed-system policy\n"
        "  --make-include PATH          Make include with KCONFIG_<symbol> variables\n"
        "  --guard NAME                 include guard for --out-header\n"
        "  --installer-guard NAME       include guard for --out-installer-header\n"
        "  --require-license SYMBOL     symbol driving LEONOS_LICENSE_REQUIRE\n"
        "  --installer-require-license SYMBOL\n"
        "                               symbol driving the installer's licence gate\n"
        "  --help                       show this message\n"
        "\n"
        "Each output is published atomically and only when its content changes.\n",
        TOOL_NAME);
}

static int parse_options(int argc, char **argv, struct options *options)
{
    int index;

    memset(options, 0, sizeof(*options));
    options->guard = "LEONOS4_AUTOCONF_H";
    options->installer_guard = "LEONOS4_AUTOCONF_INSTALLER_H";

    for (index = 1; index < argc; index++) {
        char *argument = argv[index];

        if (strcmp(argument, "--help") == 0) {
            usage();
            exit(0);
        }
        if (index + 1 >= argc) {
            return report("'%s' requires a value", argument);
        }
        if (strcmp(argument, "--input") == 0) {
            options->input = argv[++index];
        } else if (strcmp(argument, "--out-header") == 0) {
            options->out_header = argv[++index];
        } else if (strcmp(argument, "--out-installer-header") == 0) {
            options->out_installer_header = argv[++index];
        } else if (strcmp(argument, "--make-include") == 0) {
            options->out_make = argv[++index];
        } else if (strcmp(argument, "--guard") == 0) {
            options->guard = argv[++index];
        } else if (strcmp(argument, "--installer-guard") == 0) {
            options->installer_guard = argv[++index];
        } else if (strcmp(argument, "--require-license") == 0) {
            options->require_license = argv[++index];
        } else if (strcmp(argument, "--installer-require-license") == 0) {
            options->installer_require_license = argv[++index];
        } else {
            return report("unrecognised argument '%s'; try --help", argument);
        }
    }

    if (options->input == NULL) {
        return report("--input is required");
    }
    if (options->out_header == NULL && options->out_installer_header == NULL &&
        options->out_make == NULL) {
        return report("at least one output option is required");
    }
    return 0;
}

static int is_key_character(char character)
{
    return (character >= 'A' && character <= 'Z') ||
        (character >= '0' && character <= '9') || character == '_';
}

static int valid_key(const char *text, size_t length)
{
    size_t index;

    if (length <= KEY_PREFIX_SIZE + 1 ||
        strncmp(text, KEY_PREFIX, KEY_PREFIX_SIZE) != 0) {
        return 0;
    }
    for (index = KEY_PREFIX_SIZE; index < length; index++) {
        if (!is_key_character(text[index])) {
            return 0;
        }
    }
    return text[length - 1] != '_';
}

static char *duplicate(const char *text, size_t length)
{
    char *copy = malloc(length + 1u);

    if (copy == NULL) {
        errno = ENOMEM;
        return NULL;
    }
    memcpy(copy, text, length);
    copy[length] = '\0';
    return copy;
}

/* Decodes a quoted Kconfig value into the exact text the C header must carry, so
 * a value containing a quote or a backslash survives the round trip. */
static char *decode_quoted(const char *text, size_t length)
{
    struct byte_buffer out = { NULL, 0, 0 };
    size_t index;

    for (index = 0; index < length; index++) {
        char character = text[index];

        if (character != '\\') {
            if (append(&out, &character, 1) != 0) {
                goto failure;
            }
            continue;
        }
        if (index + 1 >= length) {
            errno = EINVAL;
            goto failure;
        }
        index++;
        if (append(&out, &text[index], 1) != 0) {
            goto failure;
        }
    }
    if (append(&out, "\0", 1) != 0) {
        goto failure;
    }
    out.len--;
    return (char *)out.data;

failure:
    buffer_destroy(&out);
    return NULL;
}

static enum value_kind kind_of(const char *value, size_t length)
{
    size_t index;

    if (length == 1 && (value[0] == 'y' || value[0] == 'n')) {
        return KIND_BOOL;
    }
    if (length >= 2 && value[0] == '"' && value[length - 1] == '"') {
        return KIND_STRING;
    }
    if (length == 0) {
        return KIND_INVALID;
    }
    for (index = 0; index < length; index++) {
        char character = value[index];
        int digit_or_letter = (character >= '0' && character <= '9') ||
            (character >= 'a' && character <= 'f') ||
            (character >= 'A' && character <= 'F') ||
            character == 'x' || character == 'X';

        if (!digit_or_letter && !(character == '-' && index == 0)) {
            return KIND_INVALID;
        }
    }
    return KIND_NUMBER;
}

static int symbol_compare(const void *left, const void *right)
{
    const struct symbol *a = left;
    const struct symbol *b = right;

    return strcmp(a->key, b->key);
}

static void table_free(struct byte_buffer *table, size_t count)
{
    size_t index;

    for (index = 0; index < count; index++) {
        free(((struct symbol *)table->data)[index].key);
        free(((struct symbol *)table->data)[index].value);
    }
    buffer_destroy(table);
}

/* A second assignment that disagrees is an error rather than a last-write-wins
 * silently: a stale profile flipping a feature off must be visible. */
static int add_symbol(struct byte_buffer *table, size_t *count,
                      const char *key, size_t key_length,
                      const char *value, size_t value_length, int line_number)
{
    size_t index;
    struct symbol *symbols;
    struct symbol *fresh;
    char *decoded;
    enum value_kind kind;

    symbols = (struct symbol *)table->data;
    for (index = 0; index < *count; index++) {
        if (strlen(symbols[index].key) == key_length &&
            strncmp(symbols[index].key, key, key_length) == 0) {
            if (strlen(symbols[index].value) == value_length &&
                strncmp(symbols[index].value, value, value_length) == 0) {
                return 0;
            }
            return report("line %d: %.*s is assigned twice with different values",
                line_number, (int)key_length, key);
        }
    }

    kind = kind_of(value, value_length);
    if (kind == KIND_INVALID) {
        return report("line %d: cannot interpret the value of %.*s",
            line_number, (int)key_length, key);
    }
    if (kind == KIND_STRING) {
        decoded = decode_quoted(value + 1, value_length - 2);
    } else {
        decoded = duplicate(value, value_length);
    }
    if (decoded == NULL) {
        return report("line %d: malformed value for %.*s: %s",
            line_number, (int)key_length, key, strerror(errno));
    }
    if (buffer_reserve(table, (*count + 1u) * sizeof(struct symbol)) != 0) {
        free(decoded);
        return report("out of memory");
    }
    fresh = &((struct symbol *)table->data)[*count];
    fresh->key = duplicate(key, key_length);
    fresh->value = decoded;
    fresh->kind = kind;
    if (fresh->key == NULL) {
        free(decoded);
        return report("out of memory");
    }
    (*count)++;
    return 0;
}

static int load_config(const char *path, struct byte_buffer *table, size_t *count)
{
    struct byte_buffer text = { NULL, 0, 0 };
    int descriptor = open(path, O_RDONLY);
    char *cursor;
    int line_number = 0;
    int status = 0;

    if (descriptor < 0) {
        return report("cannot read %s: %s", path, strerror(errno));
    }
    for (;;) {
        size_t chunk = 8192u;
        ssize_t got;
        size_t used = text.len;

        if (buffer_reserve(&text, used + chunk) != 0) {
            status = report("out of memory");
            break;
        }
        got = read(descriptor, text.data + used, chunk);
        if (got < 0) {
            if (errno == EINTR) {
                continue;
            }
            status = report("cannot read %s: %s", path, strerror(errno));
            break;
        }
        if (got == 0) {
            break;
        }
        text.len = used + (size_t)got;
    }
    if (close(descriptor) != 0 && status == 0) {
        status = report("cannot finish reading %s: %s", path, strerror(errno));
    }
    if (status != 0) {
        buffer_destroy(&text);
        return status;
    }
    if (append(&text, "\0", 1) != 0) {
        buffer_destroy(&text);
        return report("out of memory");
    }

    cursor = (char *)text.data;
    while (*cursor != '\0') {
        char *newline = strchr(cursor, '\n');
        char *line = cursor;
        size_t line_length;
        char *assignment;
        size_t comment_length;

        if (newline != NULL) {
            *newline = '\0';
            cursor = newline + 1;
        } else {
            cursor = line + strlen(line);
        }
        line_number++;
        line_length = strlen(line);
        while (line_length > 0 && (line[line_length - 1] == '\r' ||
                line[line_length - 1] == ' ' || line[line_length - 1] == '\t')) {
            line_length--;
            line[line_length] = '\0';
        }
        if (line_length == 0) {
            continue;
        }
        if (line[0] == '#') {
            /* Only the exact "# CONFIG_X is not set" form carries meaning;
             * anything else beginning with a hash is a comment. */
            comment_length = strlen(NOT_SET_TEXT);
            if (line_length > 2 + KEY_PREFIX_SIZE + comment_length &&
                strncmp(line + line_length - comment_length, NOT_SET_TEXT,
                    comment_length) == 0) {
                char *name = line + 2;
                size_t name_length = line_length - 2 - comment_length;

                while (name_length > 0 && (name[name_length - 1] == ' ' ||
                        name[name_length - 1] == '\t')) {
                    name_length--;
                }
                if (!valid_key(name, name_length)) {
                    status = report("line %d: '%s' is not a valid CONFIG_ symbol",
                        line_number, line);
                    break;
                }
                status = add_symbol(table, count, name, name_length, "n", 1, line_number);
                if (status != 0) {
                    break;
                }
            }
            continue;
        }
        if (line[0] == '/' && line[1] == '*') {
            continue;
        }
        assignment = strchr(line, '=');
        if (assignment == NULL) {
            status = report("line %d: '%s' is neither an assignment nor a comment",
                line_number, line);
            break;
        }
        {
            size_t key_length = (size_t)(assignment - line);
            const char *value = assignment + 1;
            size_t value_length = strlen(value);

            while (key_length > 0 && (line[key_length - 1] == ' ' ||
                    line[key_length - 1] == '\t')) {
                key_length--;
            }
            if (!valid_key(line, key_length)) {
                status = report("line %d: '%.*s' is not a valid CONFIG_ symbol",
                    line_number, (int)key_length, line);
                break;
            }
            while (value_length > 0 && (*value == ' ' || *value == '\t')) {
                value++;
                value_length--;
            }
            status = add_symbol(table, count, line, key_length, value, value_length,
                line_number);
            if (status != 0) {
                break;
            }
        }
    }

    free(text.data);
    return status;
}

static const struct symbol *find_symbol(const struct symbol *symbols, size_t count,
                                        const char *key)
{
    size_t index;

    if (key == NULL) {
        return NULL;
    }
    for (index = 0; index < count; index++) {
        if (strcmp(symbols[index].key, key) == 0) {
            return &symbols[index];
        }
    }
    return NULL;
}

/* Escapes the two characters that would otherwise be re-read as Make syntax:
 * '#' starts a comment and '$' starts an expansion. */
static int append_make_value(struct byte_buffer *out, const char *value)
{
    const char *cursor;

    for (cursor = value; *cursor != '\0'; cursor++) {
        if (*cursor == '#') {
            if (append_text(out, "\\#") != 0) {
                return -1;
            }
        } else if (*cursor == '$') {
            if (append_text(out, "$$") != 0) {
                return -1;
            }
        } else if (append(out, cursor, 1) != 0) {
            return -1;
        }
    }
    return 0;
}

static int render_header(struct byte_buffer *out, const char *guard,
                         const struct symbol *symbols, size_t count,
                         const char *require_license)
{
    size_t index;
    const struct symbol *licence;

    if (appendf(out, "/* Generated by %s from the Kconfig .config. Do not edit. */\n",
            TOOL_NAME) != 0 ||
        appendf(out, "#ifndef %s\n#define %s\n\n", guard, guard) != 0) {
        return -1;
    }
    for (index = 0; index < count; index++) {
        const struct symbol *symbol = &symbols[index];

        if (strcmp(symbol->value, "n") == 0) {
            continue;
        }
        if (symbol->kind == KIND_BOOL) {
            if (appendf(out, "#define %s 1\n", symbol->key) != 0) {
                return -1;
            }
        } else if (symbol->kind == KIND_STRING) {
            if (appendf(out, "#define %s \"%s\"\n", symbol->key, symbol->value) != 0) {
                return -1;
            }
        } else if (appendf(out, "#define %s %s\n", symbol->key, symbol->value) != 0) {
            return -1;
        }
    }
    licence = find_symbol(symbols, count, require_license);
    if (appendf(out, "\n#define LEONOS_LICENSE_REQUIRE %d\n\n#endif /* %s */\n",
            (licence != NULL && strcmp(licence->value, "y") == 0) ? 1 : 0, guard) != 0) {
        return -1;
    }
    return 0;
}

static int render_make_include(struct byte_buffer *out,
                               const struct symbol *symbols, size_t count)
{
    size_t index;

    if (appendf(out, "# Generated by %s from the Kconfig .config. Do not edit.\n",
            TOOL_NAME) != 0) {
        return -1;
    }
    for (index = 0; index < count; index++) {
        if (appendf(out, "KCONFIG_%s :=", symbols[index].key) != 0 ||
            append_make_value(out, symbols[index].value) != 0 ||
            append_text(out, "\n") != 0) {
            return -1;
        }
    }
    return 0;
}

static int publish(const char *path, const struct byte_buffer *out)
{
    if (write_file_if_changed(path, out->data, out->len, 0644u) != 0) {
        return report("cannot publish %s: %s", path, strerror(errno));
    }
    return 0;
}

int main(int argc, char **argv)
{
    struct options options;
    struct byte_buffer table = { NULL, 0, 0 };
    struct byte_buffer header = { NULL, 0, 0 };
    struct byte_buffer installer_header = { NULL, 0, 0 };
    struct byte_buffer make_include = { NULL, 0, 0 };
    size_t count = 0;
    int status;

    status = parse_options(argc, argv, &options);
    if (status != 0) {
        return status;
    }
    status = load_config(options.input, &table, &count);
    if (status != 0) {
        table_free(&table, count);
        return status;
    }
    if (count == 0) {
        table_free(&table, count);
        return report("%s contains no configuration symbols", options.input);
    }
    qsort(table.data, count, sizeof(struct symbol), symbol_compare);

    {
        const struct symbol *symbols = (const struct symbol *)table.data;

        if ((options.out_header != NULL &&
                render_header(&header, options.guard, symbols, count,
                    options.require_license) != 0) ||
            (options.out_installer_header != NULL &&
                render_header(&installer_header, options.installer_guard, symbols,
                    count, options.installer_require_license) != 0) ||
            (options.out_make != NULL &&
                render_make_include(&make_include, symbols, count) != 0)) {
            status = report("cannot render output: %s", strerror(errno));
        }
    }

    if (status == 0 && options.out_header != NULL) {
        status = publish(options.out_header, &header);
    }
    if (status == 0 && options.out_installer_header != NULL) {
        status = publish(options.out_installer_header, &installer_header);
    }
    if (status == 0 && options.out_make != NULL) {
        status = publish(options.out_make, &make_include);
    }

    buffer_destroy(&header);
    buffer_destroy(&installer_header);
    buffer_destroy(&make_include);
    table_free(&table, count);
    return status;
}
