/*
 * Strict JSON reader. See json.h for the contract this file implements.
 */

#include "tools/host/manifest/json.h"

#include "tools/host/common/buffer.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define JSON_CONTAINER_GROWTH 8u

typedef struct {
    const unsigned char *data;
    size_t length;
    size_t offset;
    size_t line;
    size_t column;
    char *error;
    size_t error_size;
} scanner;

static void json_reset(json_value *value)
{
    value->type = JSON_NULL;
    value->text = NULL;
    value->number = 0.0;
    value->children = NULL;
    value->keys = NULL;
    value->child_count = 0;
    value->child_capacity = 0;
}

static void fail(scanner *sc, const char *what)
{
    int written;

    if (sc->error == NULL || sc->error_size == 0) {
        return;
    }
    /* snprintf always terminates within error_size, so a caller buffer that is
     * too small for the position prefix and the reason still yields a valid,
     * non-empty string. */
    written = snprintf(sc->error, sc->error_size, "line %zu column %zu: %s", sc->line,
        sc->column, what);
    if (written < 0) {
        sc->error[0] = '\0';
    }
}

static int at_end(const scanner *sc)
{
    return sc->offset >= sc->length;
}

static unsigned char peek(const scanner *sc)
{
    return at_end(sc) ? 0u : sc->data[sc->offset];
}

static unsigned char take(scanner *sc)
{
    unsigned char byte = sc->data[sc->offset];

    sc->offset++;
    if (byte == '\n') {
        sc->line++;
        sc->column = 1;
    } else {
        sc->column++;
    }
    return byte;
}

static int skip_whitespace(scanner *sc)
{
    for (;;) {
        unsigned char byte;

        /* End of input is not whitespace's problem: every caller inspects the
         * peeked byte afterwards and reports its own error. Reporting failure
         * here would make a well-formed document that ends without trailing
         * whitespace look truncated. */
        if (at_end(sc)) {
            return 0;
        }
        byte = peek(sc);
        if (byte != ' ' && byte != '\t' && byte != '\n' && byte != '\r') {
            return 0;
        }
        (void)take(sc);
    }
}

/* Appends one UTF-8 sequence for a code point. Surrogates are rejected: they
 * only ever arrive as a pair, which the caller handles. */
static int append_codepoint(struct byte_buffer *out, unsigned long code)
{
    size_t needed = out->len + 4u;
    size_t start;

    if (code >= 0xD800ul && code <= 0xDFFFul) {
        return -1;
    }
    if (buffer_reserve(out, needed) != 0) {
        return -1;
    }
    start = out->len;
    if (code < 0x80ul) {
        out->data[start] = (unsigned char)code;
        out->len = start + 1u;
    } else if (code < 0x800ul) {
        out->data[start] = (unsigned char)(0xC0ul | (code >> 6));
        out->data[start + 1u] = (unsigned char)(0x80ul | (code & 0x3Ful));
        out->len = start + 2u;
    } else if (code < 0x10000ul) {
        out->data[start] = (unsigned char)(0xE0ul | (code >> 12));
        out->data[start + 1u] = (unsigned char)(0x80ul | ((code >> 6) & 0x3Ful));
        out->data[start + 2u] = (unsigned char)(0x80ul | (code & 0x3Ful));
        out->len = start + 3u;
    } else if (code <= 0x10FFFFul) {
        out->data[start] = (unsigned char)(0xF0ul | (code >> 18));
        out->data[start + 1u] = (unsigned char)(0x80ul | ((code >> 12) & 0x3Ful));
        out->data[start + 2u] = (unsigned char)(0x80ul | ((code >> 6) & 0x3Ful));
        out->data[start + 3u] = (unsigned char)(0x80ul | (code & 0x3Ful));
        out->len = start + 4u;
    } else {
        return -1;
    }
    return 0;
}

static int hex_digit(unsigned char byte, unsigned long *out)
{
    if (byte >= '0' && byte <= '9') {
        *out = (unsigned long)(byte - '0');
        return 0;
    }
    if (byte >= 'a' && byte <= 'f') {
        *out = (unsigned long)(byte - 'a') + 10ul;
        return 0;
    }
    if (byte >= 'A' && byte <= 'F') {
        *out = (unsigned long)(byte - 'A') + 10ul;
        return 0;
    }
    return -1;
}

static int read_hex4(scanner *sc, unsigned long *out)
{
    unsigned long value = 0;
    size_t index;

    for (index = 0; index < 4u; index++) {
        unsigned long digit;

        if (at_end(sc) || hex_digit(peek(sc), &digit) != 0) {
            fail(sc, "malformed \\u escape");
            return -1;
        }
        value = (value << 4) | digit;
        (void)take(sc);
    }
    *out = value;
    return 0;
}

static int parse_string_raw(scanner *sc, char **out)
{
    struct byte_buffer text = {NULL, 0, 0};
    unsigned char byte;

    if (peek(sc) != '"') {
        fail(sc, "expected a string");
        return -1;
    }
    (void)take(sc);
    for (;;) {
        if (at_end(sc)) {
            fail(sc, "unterminated string");
            goto error;
        }
        byte = take(sc);
        if (byte == '"') {
            break;
        }
        if (byte < 0x20u) {
            fail(sc, "control character inside a string");
            goto error;
        }
        if (byte != '\\') {
            if (buffer_reserve(&text, text.len + 2u) != 0) {
                fail(sc, "out of memory");
                goto error;
            }
            text.data[text.len++] = byte;
            continue;
        }
        if (at_end(sc)) {
            fail(sc, "unterminated escape");
            goto error;
        }
        byte = take(sc);
        {
            unsigned long code;
            char simple;

            switch (byte) {
            case '"': simple = '"'; break;
            case '\\': simple = '\\'; break;
            case '/': simple = '/'; break;
            case 'b': simple = '\b'; break;
            case 'f': simple = '\f'; break;
            case 'n': simple = '\n'; break;
            case 'r': simple = '\r'; break;
            case 't': simple = '\t'; break;
            case 'u':
                if (read_hex4(sc, &code) != 0) {
                    goto error;
                }
                if (code >= 0xD800ul && code <= 0xDBFFul) {
                    unsigned long low;

                    if (at_end(sc) || take(sc) != '\\') {
                        fail(sc, "unpaired high surrogate");
                        goto error;
                    }
                    if (at_end(sc) || take(sc) != 'u') {
                        fail(sc, "unpaired high surrogate");
                        goto error;
                    }
                    if (read_hex4(sc, &low) != 0) {
                        goto error;
                    }
                    if (low < 0xDC00ul || low > 0xDFFFul) {
                        fail(sc, "unpaired high surrogate");
                        goto error;
                    }
                    code = 0x10000ul + ((code - 0xD800ul) << 10) + (low - 0xDC00ul);
                } else if (code >= 0xDC00ul && code <= 0xDFFFul) {
                    fail(sc, "unpaired low surrogate");
                    goto error;
                }
                if (code == 0ul) {
                    fail(sc, "NUL code point is not representable");
                    goto error;
                }
                if (append_codepoint(&text, code) != 0) {
                    fail(sc, "invalid code point");
                    goto error;
                }
                continue;
            default:
                fail(sc, "unknown escape");
                goto error;
            }
            if (buffer_reserve(&text, text.len + 2u) != 0) {
                fail(sc, "out of memory");
                goto error;
            }
            text.data[text.len++] = (unsigned char)simple;
        }
    }
    if (buffer_reserve(&text, text.len + 1u) != 0) {
        fail(sc, "out of memory");
        goto error;
    }
    text.data[text.len] = '\0';
    *out = (char *)text.data;
    return 0;

error:
    buffer_destroy(&text);
    return -1;
}

/* Appends `child` to `parent`, taking ownership of `key` (which is moved, not
 * copied, whether or not this succeeds). */
static int push_child(json_value *parent, char *key, json_value *child)
{
    size_t needed = parent->child_count + 1u;

    if (needed > parent->child_capacity) {
        size_t capacity = parent->child_capacity == 0
            ? JSON_CONTAINER_GROWTH : parent->child_capacity * 2u;
        json_value *children;
        char **keys;

        if (capacity < needed) {
            capacity = needed;
        }
        children = realloc(parent->children, capacity * sizeof(*children));
        if (children == NULL) {
            free(key);
            return -1;
        }
        parent->children = children;
        if (parent->type == JSON_OBJECT) {
            keys = realloc(parent->keys, capacity * sizeof(*keys));
            if (keys == NULL) {
                free(key);
                return -1;
            }
            parent->keys = keys;
        }
        parent->child_capacity = capacity;
    }
    parent->children[parent->child_count] = *child;
    if (parent->type == JSON_OBJECT) {
        parent->keys[parent->child_count] = key;
    } else {
        free(key);
    }
    parent->child_count++;
    json_reset(child);
    return 0;
}

static int has_key(const json_value *object, const char *key)
{
    size_t index;

    for (index = 0; index < object->child_count; index++) {
        if (strcmp(object->keys[index], key) == 0) {
            return 1;
        }
    }
    return 0;
}

static int parse_literal(scanner *sc, json_value *out)
{
    static const struct {
        const char *text;
        json_type type;
    } literals[] = {
        {"true", JSON_TRUE}, {"false", JSON_FALSE}, {"null", JSON_NULL},
    };
    size_t index;

    for (index = 0; index < sizeof(literals) / sizeof(literals[0]); index++) {
        const char *word = literals[index].text;
        size_t word_length = strlen(word);

        if (sc->length - sc->offset < word_length) {
            continue;
        }
        if (memcmp(&sc->data[sc->offset], word, word_length) != 0) {
            continue;
        }
        sc->offset += word_length;
        sc->column += word_length;
        out->type = literals[index].type;
        return 0;
    }
    fail(sc, "unexpected token");
    return -1;
}

static int parse_number(scanner *sc, json_value *out)
{
    size_t start = sc->offset;
    size_t index;
    char *end;
    double value;
    char *text;

    /* JSON grammar: -?(0|[1-9][0-9]*)(\.[0-9]+)?([eE][+-]?[0-9]+)? */
    if (peek(sc) == '-') {
        (void)take(sc);
    }
    if (at_end(sc)) {
        fail(sc, "malformed number");
        return -1;
    }
    if (peek(sc) == '0') {
        (void)take(sc);
    } else if (peek(sc) >= '1' && peek(sc) <= '9') {
        while (!at_end(sc) && peek(sc) >= '0' && peek(sc) <= '9') {
            (void)take(sc);
        }
    } else {
        fail(sc, "malformed number");
        return -1;
    }
    if (!at_end(sc) && peek(sc) == '.') {
        (void)take(sc);
        index = 0;
        while (!at_end(sc) && peek(sc) >= '0' && peek(sc) <= '9') {
            (void)take(sc);
            index++;
        }
        if (index == 0) {
            fail(sc, "malformed number");
            return -1;
        }
    }
    if (!at_end(sc) && (peek(sc) == 'e' || peek(sc) == 'E')) {
        (void)take(sc);
        if (!at_end(sc) && (peek(sc) == '+' || peek(sc) == '-')) {
            (void)take(sc);
        }
        index = 0;
        while (!at_end(sc) && peek(sc) >= '0' && peek(sc) <= '9') {
            (void)take(sc);
            index++;
        }
        if (index == 0) {
            fail(sc, "malformed number");
            return -1;
        }
    }
    text = calloc(sc->offset - start + 1u, 1u);
    if (text == NULL) {
        fail(sc, "out of memory");
        return -1;
    }
    memcpy(text, &sc->data[start], sc->offset - start);
    errno = 0;
    value = strtod(text, &end);
    if (end == NULL || *end != '\0' || errno == ERANGE) {
        free(text);
        fail(sc, "number out of range");
        return -1;
    }
    free(text);
    out->type = JSON_NUMBER;
    out->number = value;
    return 0;
}

static int parse_value(scanner *sc, size_t depth, json_value *out);

static int parse_container(scanner *sc, size_t depth, json_value *out, int is_object)
{
    unsigned char closer = is_object ? '}' : ']';
    json_value child;

    out->type = is_object ? JSON_OBJECT : JSON_ARRAY;
    (void)take(sc); /* the opener */
    if (skip_whitespace(sc) != 0) {
        return -1;
    }
    if (peek(sc) == closer) {
        (void)take(sc);
        return 0;
    }
    for (;;) {
        char *key = NULL;

        json_reset(&child);
        if (is_object) {
            if (skip_whitespace(sc) != 0) {
                return -1;
            }
            if (parse_string_raw(sc, &key) != 0) {
                return -1;
            }
            if (has_key(out, key)) {
                free(key);
                fail(sc, "duplicate object key");
                return -1;
            }
            if (skip_whitespace(sc) != 0) {
                free(key);
                return -1;
            }
            if (peek(sc) != ':') {
                free(key);
                fail(sc, "expected ':' after an object key");
                return -1;
            }
            (void)take(sc);
        }
        if (parse_value(sc, depth + 1u, &child) != 0) {
            free(key);
            json_free(&child);
            return -1;
        }
        if (push_child(out, key, &child) != 0) {
            json_free(&child);
            fail(sc, "out of memory");
            return -1;
        }
        if (skip_whitespace(sc) != 0) {
            return -1;
        }
        if (peek(sc) == ',') {
            (void)take(sc);
            continue;
        }
        if (peek(sc) == closer) {
            (void)take(sc);
            return 0;
        }
        fail(sc, "expected ',' or the end of a container");
        return -1;
    }
}

static int parse_value(scanner *sc, size_t depth, json_value *out)
{
    unsigned char byte;

    if (depth > JSON_MAX_DEPTH) {
        fail(sc, "nesting deeper than the supported limit");
        return -1;
    }
    if (skip_whitespace(sc) != 0) {
        return -1;
    }
    byte = peek(sc);
    switch (byte) {
    case '{':
        return parse_container(sc, depth, out, 1);
    case '[':
        return parse_container(sc, depth, out, 0);
    case '"': {
        if (parse_string_raw(sc, &out->text) != 0) {
            return -1;
        }
        out->type = JSON_STRING;
        return 0;
    }
    case '-':
    case '0': case '1': case '2': case '3': case '4':
    case '5': case '6': case '7': case '8': case '9':
        return parse_number(sc, out);
    default:
        return parse_literal(sc, out);
    }
}

int json_parse(const char *data, size_t length, json_value *root, char *error,
    size_t error_size)
{
    scanner sc;

    json_reset(root);
    if (error != NULL && error_size > 0) {
        error[0] = '\0';
    }
    if (data == NULL) {
        if (error != NULL && error_size > 0) {
            (void)snprintf(error, error_size, "no input to parse");
        }
        return -1;
    }
    sc.data = (const unsigned char *)data;
    sc.length = length;
    sc.offset = 0;
    sc.line = 1;
    sc.column = 1;
    sc.error = error;
    sc.error_size = error_size;

    if (parse_value(&sc, 1, root) != 0) {
        return -1;
    }
    if (skip_whitespace(&sc) != 0) {
        return -1;
    }
    if (!at_end(&sc)) {
        fail(&sc, "trailing content after the document");
        return -1;
    }
    return 0;
}

static int key_equals(const char *stored, const char *start, size_t length)
{
    return strlen(stored) == length && memcmp(stored, start, length) == 0;
}

static const json_value *member_by_name(const json_value *value, const char *start,
    size_t length)
{
    size_t index;

    if (value->type != JSON_OBJECT) {
        return NULL;
    }
    for (index = 0; index < value->child_count; index++) {
        if (key_equals(value->keys[index], start, length)) {
            return &value->children[index];
        }
    }
    return NULL;
}

const json_value *json_at(const json_value *value, const char *path)
{
    const char *cursor = path;

    if (value == NULL || path == NULL) {
        return NULL;
    }
    while (*cursor != '\0') {
        const char *start;
        size_t length;

        if (*cursor == '/') {
            cursor++;
            continue;
        }
        if (*cursor == '[') {
            const char *digits = cursor + 1;
            char *end;
            unsigned long index;

            if (*digits < '0' || *digits > '9') {
                return NULL;
            }
            errno = 0;
            index = strtoul(digits, &end, 10);
            if (errno != 0 || end == digits || *end != ']') {
                return NULL;
            }
            cursor = end + 1;
            if (value->type != JSON_ARRAY || (size_t)index >= value->child_count) {
                return NULL;
            }
            value = &value->children[index];
            continue;
        }
        start = cursor;
        while (*cursor != '\0' && *cursor != '/' && *cursor != '[') {
            cursor++;
        }
        length = (size_t)(cursor - start);
        value = member_by_name(value, start, length);
        if (value == NULL) {
            return NULL;
        }
    }
    return value;
}

void json_free(json_value *value)
{
    size_t index;

    if (value == NULL) {
        return;
    }
    free(value->text);
    for (index = 0; index < value->child_count; index++) {
        json_free(&value->children[index]);
        if (value->keys != NULL) {
            free(value->keys[index]);
        }
    }
    free(value->children);
    free(value->keys);
    json_reset(value);
}

size_t json_child_count(const json_value *value)
{
    if (value == NULL) {
        return 0;
    }
    return value->child_count;
}

const json_value *json_child(const json_value *value, size_t index)
{
    if (value == NULL || index >= value->child_count) {
        return NULL;
    }
    return &value->children[index];
}

const json_value *json_member(const json_value *value, const char *key)
{
    size_t index;

    if (value == NULL || key == NULL || value->type != JSON_OBJECT) {
        return NULL;
    }
    for (index = 0; index < value->child_count; index++) {
        if (strcmp(value->keys[index], key) == 0) {
            return &value->children[index];
        }
    }
    return NULL;
}

const char *json_text(const json_value *value)
{
    if (value == NULL || value->type != JSON_STRING) {
        return NULL;
    }
    return value->text;
}

int json_int(const json_value *value, long *out)
{
    long converted;

    if (value == NULL || out == NULL || value->type != JSON_NUMBER) {
        return -1;
    }
    if (value->number != (double)(long)value->number) {
        return -1;
    }
    converted = (long)value->number;
    if ((double)converted != value->number) {
        return -1;
    }
    *out = converted;
    return 0;
}
