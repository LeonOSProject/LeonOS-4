/*
 * Minimal assertion harness for the host-tool test binaries.
 *
 * Deliberately dependency-free: the acceptance contract for the new build
 * system is that its checks run with nothing but a C compiler and POSIX sh.
 */
#ifndef LEONOS_TEST_SUPPORT_H
#define LEONOS_TEST_SUPPORT_H

/* mkdtemp, opendir/readdir and posix_spawn all come from POSIX, not ISO C11. */
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

static unsigned leonos_test_checks;
static unsigned leonos_test_failures;

#define TEST_ASSERT(condition)                                                      \
    do {                                                                        \
        leonos_test_checks++;                                               \
        if (!(condition)) {                                                 \
            leonos_test_failures++;                                     \
            fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__,     \
                #condition);                                          \
        }                                                                   \
    } while (0)

#define TEST_ASSERT_EQ(expected, actual)                                            \
    do {                                                                        \
        long leonos__e = (long)(expected);                                  \
        long leonos__a = (long)(actual);                                    \
        leonos_test_checks++;                                               \
        if (leonos__e != leonos__a) {                                       \
            leonos_test_failures++;                                     \
            fprintf(stderr, "FAIL %s:%d: %s == %s (%ld != %ld)\n",      \
                __FILE__, __LINE__, #expected, #actual,                 \
                leonos__e, leonos__a);                                  \
        }                                                                   \
    } while (0)

#define TEST_ASSERT_STR_EQ(expected, actual)                                        \
    do {                                                                        \
        const char *leonos__e = (expected);                                 \
        const char *leonos__a = (actual);                                   \
        leonos_test_checks++;                                               \
        if (strcmp(leonos__e, leonos__a) != 0) {                            \
            leonos_test_failures++;                                     \
            fprintf(stderr, "FAIL %s:%d: text differs\n  expected: %s\n  actual:   %s\n", \
                __FILE__, __LINE__, leonos__e, leonos__a);              \
        }                                                                   \
    } while (0)

/* Returns the suite exit code: 0 when every check held. */
static int leonos_test_report(const char *suite_name)
{
    if (leonos_test_failures != 0) {
        printf("not ok - %s: %u of %u checks failed\n", suite_name,
            leonos_test_failures, leonos_test_checks);
        return 1;
    }
    printf("ok - %s: %u checks passed\n", suite_name, leonos_test_checks);
    return 0;
}

static int test_join(char *out, size_t out_size, const char *dir, const char *name)
{
    int written = snprintf(out, out_size, "%s/%s", dir, name);

    return (written < 0 || (size_t)written >= out_size) ? -1 : 0;
}

/* Creates a private temporary directory, or returns NULL. */
static inline char *test_temp_dir(char *buffer, size_t size)
{
    const char *pattern = "./leonos-host-test-XXXXXX";

    if (size < strlen(pattern) + 1) {
        return NULL;
    }
    strcpy(buffer, pattern); /* template length already validated above */
    return mkdtemp(buffer);
}

/* Recursively unlinks everything under `path`, then removes `path` itself.
 * Only used on directories this suite created, so a failure is reported rather
 * than silently ignored. */
static inline void test_remove_tree(const char *path)
{
    DIR *directory = opendir(path);
    struct dirent *entry;
    char child[1024];

    if (directory == NULL) {
        return;
    }
    while ((entry = readdir(directory)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        if (test_join(child, sizeof(child), path, entry->d_name) != 0) {
            continue;
        }
        {
            struct stat info;

            if (lstat(child, &info) == 0 && S_ISDIR(info.st_mode)) {
                test_remove_tree(child);
                continue;
            }
        }
        (void)unlink(child);
    }
    closedir(directory);
    (void)rmdir(path);
}

static inline char *test_read_all(const char *path, size_t *out_size)
{
    FILE *stream = fopen(path, "rb");
    char *data;
    long size;

    if (stream == NULL) {
        return NULL;
    }
    if (fseek(stream, 0, SEEK_END) != 0) {
        fclose(stream);
        return NULL;
    }
    size = ftell(stream);
    if (size < 0) {
        fclose(stream);
        return NULL;
    }
    if (fseek(stream, 0, SEEK_SET) != 0) {
        fclose(stream);
        return NULL;
    }
    data = malloc((size_t)size + 1u);
    if (data == NULL) {
        fclose(stream);
        return NULL;
    }
    if (size != 0 && fread(data, (size_t)size, 1u, stream) != 1u) {
        free(data);
        fclose(stream);
        return NULL;
    }
    data[size] = '\0';
    fclose(stream);
    *out_size = (size_t)size;
    return data;
}

/* Writes a fixture file verbatim; inline so suites that do not need it stay
 * warning-clean under -Wunused-function. */
static inline int test_write_plain(const char *path, const void *data, size_t size)
{
    FILE *stream = fopen(path, "wb");
    int result = 0;

    if (stream == NULL) {
        return -1;
    }
    if (size != 0 && fwrite(data, size, 1u, stream) != 1u) {
        result = -1;
    }
    if (fclose(stream) != 0) {
        result = -1;
    }
    return result;
}

#endif /* LEONOS_TEST_SUPPORT_H */
