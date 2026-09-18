/*
 * Strict JSON reader for machine-owned build metadata.
 *
 * `configs/dependencies.lock.json` decides which upstream source tree a build
 * unpacks and which digest it accepts, so this parser is deliberately not
 * lenient: it rejects trailing commas, duplicate keys, control characters in
 * strings, unpaired surrogates, bare `NaN`, more than one document and any
 * nesting past JSON_MAX_DEPTH. A silently forgiving reader would let a
 * malformed lock entry resolve to the wrong dependency.
 *
 * Ownership: json_parse() fills a caller-owned `json_value` and every
 * allocation hangs off it, so json_free() is the single release point. A failed
 * parse leaves the root safe to free.
 */
#ifndef LEONOS_HOST_MANIFEST_JSON_H
#define LEONOS_HOST_MANIFEST_JSON_H

#include <stddef.h>

/** Maximum nesting depth. Beyond this a document is treated as hostile, not deep. */
#define JSON_MAX_DEPTH 64

typedef enum {
    JSON_NULL,
    JSON_FALSE,
    JSON_TRUE,
    JSON_NUMBER,
    JSON_STRING,
    JSON_ARRAY,
    JSON_OBJECT
} json_type;

typedef struct json_value json_value;

struct json_value {
    json_type type;
    /** JSON_STRING: the unescaped, NUL-terminated payload. */
    char *text;
    /** JSON_NUMBER: the parsed value. */
    double number;
    /** JSON_ARRAY / JSON_OBJECT: owned, contiguous children. */
    json_value *children;
    /** JSON_OBJECT: key for `children[i]`, NULL for other types. */
    char **keys;
    size_t child_count;
    size_t child_capacity;
};

/**
 * @brief Parse exactly one JSON document.
 * @param data Document bytes; need not be NUL-terminated.
 * @param length Bytes available at `data`.
 * @param root Caller-owned struct filled on success, safe to free either way.
 * @param error Optional buffer receiving a `line N column M` prefixed message.
 * @param error_size Size of `error`; a short buffer truncates, never overruns.
 * @return 0 on success, -1 on any syntax or trailing-content error.
 */
int json_parse(const char *data, size_t length, json_value *root, char *error,
    size_t error_size);

/** @brief Release everything allocated below `value`, including `value` itself. */
void json_free(json_value *value);

/** @brief Number of array elements or object members; 0 for other types. */
size_t json_child_count(const json_value *value);

/** @brief Child at `index`, or NULL when out of range. */
const json_value *json_child(const json_value *value, size_t index);

/** @brief Object member named `key`, or NULL. Arrays and scalars return NULL. */
const json_value *json_member(const json_value *value, const char *key);

/**
 * @brief Resolve a `/`-separated path such as `dependencies[3]/url`.
 * @param value Starting node.
 * @param path Object keys, optional `[n]` array indices, or both.
 * @return The addressed node, or NULL when any step is missing or mis-typed.
 */
const json_value *json_at(const json_value *value, const char *path);

/** @brief String payload, or NULL when `value` is not a string. */
const char *json_text(const json_value *value);

/**
 * @brief Read a number as an exact integer.
 * @param value Node to read.
 * @param out Receives the value on success.
 * @return 0 when `value` is a number with no fractional or exponent part,
 *         -1 otherwise.
 */
int json_int(const json_value *value, long *out);

#endif /* LEONOS_HOST_MANIFEST_JSON_H */
