/*
 * Contract tests for the JSON reader behind configs/dependencies.lock.json.
 *
 * The lock file decides which upstream source every build unpacks and which
 * artifact digest is accepted, so a parser that "looks like it works on the
 * examples" is not good enough: it has to reject the malformed inputs that
 * would otherwise silently resolve to the wrong dependency.
 */

#include "test-support.h"

#include "tools/host/manifest/json.h"

#include <errno.h>
#include <string.h>

static int parse(json_value *root, const char *text)
{
    char error[256];

    return json_parse(text, strlen(text), root, error, sizeof(error));
}

static int parse_part(json_value *root, const char *text, size_t length)
{
    char error[256];

    return json_parse(text, length, root, error, sizeof(error));
}

static void test_parses_empty_containers(void)
{
    json_value root;

    TEST_ASSERT_EQ(0, parse(&root, "{}"));
    TEST_ASSERT_EQ(JSON_OBJECT, root.type);
    TEST_ASSERT_EQ(0, json_child_count(&root));
    json_free(&root);

    TEST_ASSERT_EQ(0, parse(&root, "[]"));
    TEST_ASSERT_EQ(JSON_ARRAY, root.type);
    TEST_ASSERT_EQ(0, json_child_count(&root));
    json_free(&root);

    /* Separators and surrounding whitespace are insignificant. */
    TEST_ASSERT_EQ(0, parse(&root, "  {  \"a\" : 1 , \"b\" : [ 2 , 3 ] }  "));
    TEST_ASSERT_EQ(2, json_child_count(&root));
    json_free(&root);
}

static void test_string_escapes_decode_to_bytes(void)
{
    json_value root;
    const json_value *value;

    TEST_ASSERT_EQ(0, parse(&root,
        "{\"v\":\"a\\\"b\\\\c\\/d\\b\\f\\n\\r\\t\\u0041\\u20ac\\ud83d\\ude00\"}"));
    value = json_at(&root, "v");
    TEST_ASSERT(value != NULL);
    TEST_ASSERT_STR_EQ("a\"b\\c/d\b\f\n\r\tA\xe2\x82\xac\xf0\x9f\x98\x80", json_text(value));
    json_free(&root);
}

static void test_rejects_malformed_documents(void)
{
    static const char *const bad[] = {
        "",                          /* nothing to parse */
        "   ",                       /* whitespace only */
        "{",                         /* unterminated object */
        "[",                         /* unterminated array */
        "{\"a\"",                    /* missing value */
        "{\"a\":}",                  /* missing value after colon */
        "{a:1}",                     /* unquoted key */
        "{\"a\"=1}",                 /* wrong key/value separator */
        "{\"a\":1,}",                /* trailing comma */
        "{\"a\":1 \"b\":2}",         /* missing comma */
        "[1,2,]",                    /* trailing comma in array */
        "{\"a\":\"b\", \"a\":\"c\"}", /* duplicate key is ambiguous */
        "\"unterminated",            /* unterminated string */
        "\"tab	in\"",              /* raw control character in string */
        "\"\\x41\"",                 /* escape that JSON does not define */
        "\"\\u00\"",                 /* truncated unicode escape */
        "\"\\uD800\"",               /* unpaired high surrogate */
        "\"\\uDC00\"",               /* unpaired low surrogate */
        "01",                        /* leading zero */
        "-.5",                       /* missing integer part */
        "1.",                        /* missing fraction digits */
        "1e",                        /* missing exponent digits */
        "+1",                        /* sign JSON does not allow */
        "nul",                       /* misspelled literal */
        "true false",                /* two documents in one file */
        "{}[]",                      /* trailing content */
        "]",                         /* stray closer */
        "[007]",                     /* leading zero in a number */
    };
    size_t index;

    for (index = 0; index < sizeof(bad) / sizeof(bad[0]); index++) {
        json_value root;

        TEST_ASSERT_EQ(-1, parse_part(&root, bad[index], strlen(bad[index])));
        json_free(&root);
    }

    /* A raw NUL inside a string literal is a control character, and the only
     * way to hand it to the parser is by length. */
    {
        json_value root;
        static const char embedded[] = {'"', 'a', '\0', 'b', '"'};

        TEST_ASSERT_EQ(-1, parse_part(&root, embedded, sizeof(embedded)));
        json_free(&root);
    }
}

static void test_rejects_nesting_beyond_the_limit(void)
{
    char deep[2 * (JSON_MAX_DEPTH + 4)];
    size_t index;
    size_t depth = JSON_MAX_DEPTH + 2;
    json_value root;

    for (index = 0; index < depth; index++) {
        deep[index] = '[';
    }
    for (index = 0; index < depth; index++) {
        deep[depth + index] = ']';
    }
    deep[2 * depth] = '\0';
    TEST_ASSERT_EQ(-1, parse_part(&root, deep, strlen(deep)));
    json_free(&root);

    /* Exactly at the limit still parses, so the bound is a limit and not an
     * off-by-one that breaks real documents. */
    depth = JSON_MAX_DEPTH;
    for (index = 0; index < depth; index++) {
        deep[index] = '[';
    }
    for (index = 0; index < depth; index++) {
        deep[depth + index] = ']';
    }
    TEST_ASSERT_EQ(0, parse_part(&root, deep, 2 * depth));
    json_free(&root);
}

static void test_lookup_paths_and_types(void)
{
    json_value root;
    const json_value *dependencies;
    const json_value *entry;
    const json_value *number;
    long integral;
    char scratch[16];

    TEST_ASSERT_EQ(0, parse(&root,
        "{\"schema_version\":1,\"dependencies\":["
        "{\"id\":\"musl\",\"commit\":\"9fa28ec\",\"patches\":[],\"nested\":{\"ok\":true}},"
        "{\"id\":\"zlib\",\"commit\":\"da607da\"}]}"));

    number = json_at(&root, "schema_version");
    TEST_ASSERT(number != NULL);
    TEST_ASSERT_EQ(0, json_int(number, &integral));
    TEST_ASSERT_EQ(1, integral);
    TEST_ASSERT_EQ(-1, json_int(json_at(&root, "dependencies"), &integral));

    dependencies = json_at(&root, "dependencies");
    TEST_ASSERT_EQ(JSON_ARRAY, dependencies->type);
    TEST_ASSERT_EQ(2, json_child_count(dependencies));
    entry = json_child(dependencies, 1);
    TEST_ASSERT_STR_EQ("zlib", json_text(json_member(entry, "id")));
    TEST_ASSERT(json_member(entry, "missing") == NULL);

    /* A path descends objects and arrays; "[n]" indexes arrays. */
    entry = json_at(&root, "dependencies[0]");
    TEST_ASSERT(entry != NULL);
    TEST_ASSERT_STR_EQ("musl", json_text(json_member(entry, "id")));
    TEST_ASSERT(json_at(&root, "dependencies[9]/id") == NULL);
    TEST_ASSERT(json_at(&root, "dependencies/nope") == NULL);
    TEST_ASSERT(json_at(&root, "missing/deeper") == NULL);

    TEST_ASSERT_EQ(JSON_TRUE, json_at(entry, "nested/ok")->type);
    TEST_ASSERT(json_text(json_at(entry, "nested")) == NULL);

    /* Out-of-range child access returns NULL rather than reading past the end. */
    TEST_ASSERT(json_child(dependencies, 2) == NULL);
    snprintf(scratch, sizeof(scratch), "%s", "done");
    TEST_ASSERT_STR_EQ("done", scratch);
    json_free(&root);
}

static void test_parser_reports_a_position(void)
{
    json_value root;
    char error[256];
    char tight[4];

    error[0] = '\0';
    TEST_ASSERT_EQ(-1, json_parse("{\n  \"a\": 1,\n  \"b\": tru\n}", 24, &root,
        error, sizeof(error)));
    json_free(&root);
    TEST_ASSERT(strlen(error) > 0);
    /* The failing token is on the third line; a message without a position
     * would leave a lock-file author hunting through the file. */
    TEST_ASSERT(strstr(error, "line 3") != NULL);

    /* A caller buffer too small for the whole message must still be a
     * terminated string and must never be overrun. */
    memset(tight, 'x', sizeof(tight));
    TEST_ASSERT_EQ(-1, json_parse("{\"a\":}", 6, &root, tight, sizeof(tight)));
    json_free(&root);
    TEST_ASSERT(tight[sizeof(tight) - 1u] == '\0');
    TEST_ASSERT(strlen(tight) > 0);
}

int main(void)
{
    test_parses_empty_containers();
    test_string_escapes_decode_to_bytes();
    test_rejects_malformed_documents();
    test_rejects_nesting_beyond_the_limit();
    test_lookup_paths_and_types();
    test_parser_reports_a_position();
    return leonos_test_report("host/json");
}
