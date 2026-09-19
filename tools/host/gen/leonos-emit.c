/*
 * leonos-emit - publish a file only when its content would actually change.
 *
 * GNU Make needs "rebuild this target only if the text differs" for the command
 * signatures in plan section 6.2 and for the source manifests in section 6.1.
 * A plain redirection would move the mtime on every build and defeat that.
 */

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

#define TOOL_NAME "leonos-emit"

struct options {
    const char *input;
    const char *output;
    unsigned int mode;
};

static void usage(FILE *stream)
{
    fprintf(stream,
        "usage: %s --output PATH [--input PATH|-] [--mode OCTAL]\n"
        "\n"
        "Write the input bytes to PATH only when they differ from what is\n"
        "already there; identical content leaves the existing mtime alone.\n"
        "The write goes to a temporary in the destination directory and is\n"
        "published with rename(), so a failure never destroys the old file.\n"
        "\n"
        "  --input PATH   file to read; defaults to standard input\n"
        "  --output PATH  file to publish; its directory must exist\n"
        "  --mode OCTAL   permission bits for the published file (0644)\n"
        "  --help         show this message\n",
        TOOL_NAME);
}

#if defined(__GNUC__) || defined(__clang__)
static int fail(const char *format, ...) __attribute__((format(printf, 1, 2)));
#endif
static int fail(const char *format, ...)
{
    va_list arguments;
    int written;

    fputs(TOOL_NAME ": ", stderr);
    va_start(arguments, format);
    written = vfprintf(stderr, format, arguments);
    va_end(arguments);
    (void)written;
    fputc('\n', stderr);
    return 1;
}

static int parse_mode(const char *text, unsigned int *out_mode)
{
    char *end = NULL;
    unsigned long parsed;

    errno = 0;
    parsed = strtoul(text, &end, 8);
    if (errno != 0 || end == text || *end != '\0' || parsed > 07777UL) {
        return -1;
    }
    *out_mode = (unsigned int)parsed;
    return 0;
}

static int parse_arguments(int argc, char **argv, struct options *options)
{
    int index;

    options->input = NULL;
    options->output = NULL;
    options->mode = 0644u;

    for (index = 1; index < argc; index++) {
        char *argument = argv[index];

        if (strcmp(argument, "--help") == 0) {
            usage(stdout);
            exit(0);
        } else if (strcmp(argument, "--input") == 0 && index + 1 < argc) {
            options->input = argv[++index];
        } else if (strcmp(argument, "--output") == 0 && index + 1 < argc) {
            options->output = argv[++index];
        } else if (strcmp(argument, "--mode") == 0 && index + 1 < argc) {
            index++;
            if (parse_mode(argv[index], &options->mode) != 0) {
                return fail("--mode expects an octal value, got '%s'", argv[index]);
            }
        } else {
            return fail("unrecognised argument '%s'; try --help", argument);
        }
    }

    if (options->output == NULL) {
        return fail("--output is required");
    }
    return 0;
}

/* Reads to completion so the caller can decide whether the data is a file or a
 * pipe without the buffering differences of stdio. */
static int slurp(int descriptor, struct byte_buffer *buffer)
{
    for (;;) {
        size_t chunk = 16384u;
        ssize_t got;
        size_t used = buffer->len;

        if (buffer_reserve(buffer, used + chunk) != 0) {
            return -1;
        }
        got = read(descriptor, buffer->data + used, chunk);
        if (got < 0) {
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }
        if (got == 0) {
            return 0;
        }
        buffer->len = used + (size_t)got;
    }
}

int main(int argc, char **argv)
{
    struct options options;
    struct byte_buffer payload = { NULL, 0, 0 };
    int descriptor;
    int status;

    status = parse_arguments(argc, argv, &options);
    if (status != 0) {
        return status;
    }

    if (options.input == NULL || strcmp(options.input, "-") == 0) {
        descriptor = STDIN_FILENO;
    } else {
        descriptor = open(options.input, O_RDONLY);
        if (descriptor < 0) {
            return fail("cannot read %s: %s", options.input, strerror(errno));
        }
    }

    if (slurp(descriptor, &payload) != 0) {
        int saved = errno;

        if (descriptor != STDIN_FILENO) {
            close(descriptor);
        }
        buffer_destroy(&payload);
        return fail("cannot read input: %s", strerror(saved));
    }
    if (descriptor != STDIN_FILENO && close(descriptor) != 0) {
        int saved = errno;

        buffer_destroy(&payload);
        return fail("cannot finish reading input: %s", strerror(saved));
    }

    if (write_file_if_changed(options.output, payload.data, payload.len,
                              options.mode) != 0) {
        int saved = errno;

        buffer_destroy(&payload);
        return fail("cannot publish %s: %s", options.output, strerror(saved));
    }
    buffer_destroy(&payload);
    return 0;
}
