/*
 * leonos-boot-logo - render the boot-splash bitmap header from logo.png.
 *
 * Replaces tools/generate_boot_logo.py so the early-framebuffer asset has a
 * traceable regeneration path that runs no Python (plan section 7). The output
 * layout matches the previous generator byte for byte apart from the attribution
 * comment, so the migration can be checked by comparing headers.
 *
 * The DEFLATE step is upstream zlib's reference decoder,
 * third_party/zlib/contrib/puff/puff.c (zlib licence, see that file), used
 * unmodified: re-implementing a decompressor here would be a defect, not a
 * saving.
 */

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
#include "third_party/zlib/contrib/puff/puff.h"

#define TOOL_NAME "leonos-boot-logo"

#define PNG_SIGNATURE_SIZE 8u
#define CHUNK_HEADER_SIZE 12u
#define IHDR_SIZE 13u
#define RGBA_BYTES 4u
#define DEFAULT_SIZE 192u
#define MIN_SIZE 32u
#define MAX_SIZE 512u

struct options {
    const char *input;
    const char *output;
    unsigned long size;
};

struct image {
    unsigned long width;
    unsigned long height;
    unsigned char *pixels;
};

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
        "usage: %s --input LOGO.png --output PATH [--size N]\n"
        "\n"
        "Convert an 8-bit RGBA, non-interlaced PNG into a freestanding C header\n"
        "holding an %u x %u RGB bitmap composited over white.\n"
        "\n"
        "  --input PATH    source PNG\n"
        "  --output PATH   header to publish; published only when it changes\n"
        "  --size N        square output size in pixels (%u..%u, default %u)\n"
        "  --help          show this message\n",
        TOOL_NAME, DEFAULT_SIZE, DEFAULT_SIZE, MIN_SIZE, MAX_SIZE, DEFAULT_SIZE);
}

static int parse_options(int argc, char **argv, struct options *options)
{
    int index;

    options->input = NULL;
    options->output = NULL;
    options->size = DEFAULT_SIZE;

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
        } else if (strcmp(argument, "--output") == 0) {
            options->output = argv[++index];
        } else if (strcmp(argument, "--size") == 0) {
            char *end = NULL;
            unsigned long parsed;

            errno = 0;
            parsed = strtoul(argv[++index], &end, 10);
            if (errno != 0 || end == argv[index] || *end != '\0' ||
                parsed < MIN_SIZE || parsed > MAX_SIZE) {
                return report("--size must be between %u and %u", MIN_SIZE, MAX_SIZE);
            }
            options->size = parsed;
        } else {
            return report("unrecognised argument '%s'; try --help", argument);
        }
    }
    if (options->input == NULL || options->output == NULL) {
        return report("--input and --output are both required");
    }
    return 0;
}

static int read_entire_file(const char *path, struct byte_buffer *out)
{
    int descriptor = open(path, O_RDONLY);

    if (descriptor < 0) {
        return report("cannot read %s: %s", path, strerror(errno));
    }
    for (;;) {
        size_t chunk = 65536u;
        ssize_t got;
        size_t used = out->len;

        if (buffer_reserve(out, used + chunk) != 0) {
            close(descriptor);
            return report("out of memory");
        }
        got = read(descriptor, out->data + used, chunk);
        if (got < 0) {
            if (errno == EINTR) {
                continue;
            }
            close(descriptor);
            return report("cannot read %s: %s", path, strerror(errno));
        }
        if (got == 0) {
            break;
        }
        out->len = used + (size_t)got;
    }
    if (close(descriptor) != 0) {
        return report("cannot finish reading %s: %s", path, strerror(errno));
    }
    return 0;
}

static unsigned long read_be32(const unsigned char *bytes)
{
    return ((unsigned long)bytes[0] << 24) | ((unsigned long)bytes[1] << 16) |
        ((unsigned long)bytes[2] << 8) | (unsigned long)bytes[3];
}

static int paeth_predictor(int left, int above, int upper_left)
{
    int predictor = left + above - upper_left;
    int distance_left = abs(predictor - left);
    int distance_above = abs(predictor - above);
    int distance_upper_left = abs(predictor - upper_left);

    if (distance_left <= distance_above && distance_left <= distance_upper_left) {
        return left;
    }
    if (distance_above <= distance_upper_left) {
        return above;
    }
    return upper_left;
}

/* Reverses the per-row PNG filters in place, exactly as the old generator did,
 * so the two implementations can be compared pixel for pixel. */
static int unfilter(unsigned char *raw, unsigned long width, unsigned long height)
{
    unsigned long stride = width * RGBA_BYTES;
    unsigned char *previous = calloc((size_t)stride, 1u);
    unsigned long row;
    int status = 0;

    if (previous == NULL) {
        return report("out of memory");
    }
    for (row = 0; row < height && status == 0; row++) {
        unsigned char *base = raw + row * (stride + 1u);
        unsigned char filter_type = base[0];
        unsigned char *pixels = base + 1;
        unsigned long index;

        for (index = 0; index < stride; index++) {
            int left = (index >= RGBA_BYTES) ? pixels[index - RGBA_BYTES] : 0;
            int above = previous[index];
            int upper_left = (index >= RGBA_BYTES) ? previous[index - RGBA_BYTES] : 0;
            int value = pixels[index];

            switch (filter_type) {
            case 0:
                break;
            case 1:
                value += left;
                break;
            case 2:
                value += above;
                break;
            case 3:
                value += (left + above) / 2;
                break;
            case 4:
                value += paeth_predictor(left, above, upper_left);
                break;
            default:
                status = report("%s uses an unsupported PNG filter %u", "image",
                    (unsigned)filter_type);
                break;
            }
            pixels[index] = (unsigned char)(value & 0xff);
        }
        memcpy(previous, pixels, (size_t)stride);
    }
    free(previous);
    return status;
}

/* Walks the chunk stream, validating the header and gathering the image data.
 * Only the forms the old generator accepted are accepted here, so a re-encoded
 * logo fails loudly instead of silently producing a different splash. */
static int decode_png(const unsigned char *data, size_t length, struct image *image,
                     struct byte_buffer *scratch)
{
    static const unsigned char signature[PNG_SIGNATURE_SIZE] = {
        0x89, 'P', 'N', 'G', '\r', '\n', 0x1a, '\n'
    };
    size_t cursor = PNG_SIGNATURE_SIZE;
    int have_header = 0;
    unsigned long width = 0;
    unsigned long height = 0;
    unsigned long filtered_size;
    int inflated;

    if (length < PNG_SIGNATURE_SIZE || memcmp(data, signature, PNG_SIGNATURE_SIZE) != 0) {
        return report("input is not a PNG file");
    }
    while (cursor + CHUNK_HEADER_SIZE <= length) {
        unsigned long chunk_length = read_be32(data + cursor);
        const unsigned char *chunk_type = data + cursor + 4;
        const unsigned char *chunk_data = data + cursor + 8;
        size_t chunk_data_end;

        if (chunk_length > length - 8u - cursor) {
            return report("input contains a truncated PNG chunk");
        }
        chunk_data_end = cursor + 8u + chunk_length;

        if (memcmp(chunk_type, "IHDR", 4) == 0) {
            unsigned char compression;
            unsigned char filtering;
            unsigned char interlace;
            unsigned char bit_depth;
            unsigned char color_type;

            if (chunk_length != IHDR_SIZE) {
                return report("input has an invalid PNG header");
            }
            width = read_be32(chunk_data);
            height = read_be32(chunk_data + 4);
            bit_depth = chunk_data[8];
            color_type = chunk_data[9];
            compression = chunk_data[10];
            filtering = chunk_data[11];
            interlace = chunk_data[12];
            if (width == 0 || height == 0 || bit_depth != 8 || color_type != 6 ||
                compression != 0 || filtering != 0 || interlace != 0) {
                return report("boot logo must be a non-interlaced, 8-bit RGBA PNG");
            }
            have_header = 1;
        } else if (memcmp(chunk_type, "IDAT", 4) == 0) {
            if (!have_header) {
                return report("image data precedes the PNG header");
            }
            if (append(scratch, chunk_data, (size_t)chunk_length) != 0) {
                return report("out of memory");
            }
        } else if (memcmp(chunk_type, "IEND", 4) == 0) {
            break;
        }
        cursor = chunk_data_end + 4u;
    }
    if (!have_header || scratch->len == 0) {
        return report("input is not a complete PNG image");
    }

    /* Each row carries one filter byte ahead of its pixels. */
    if (height > SIZE_MAX / (width * RGBA_BYTES + 1u)) {
        return report("input declares an image too large to decode");
    }
    filtered_size = height * (width * RGBA_BYTES + 1u);
    if (buffer_reserve(scratch, scratch->len + (size_t)filtered_size) != 0) {
        return report("out of memory");
    }
    {
        unsigned char *raw = scratch->data + scratch->len;
        unsigned long output_room = filtered_size;
        unsigned long input_room;
        const unsigned char *input;

        /* IDAT holds a zlib stream: a two-byte CMF/FLG header followed by raw
         * DEFLATE. puff() decodes the DEFLATE part only. */
        if (scratch->len < 2u) {
            return report("PNG image data is too short to hold a zlib stream");
        }
        input = scratch->data + 2u;
        input_room = (unsigned long)(scratch->len - 2u);

        inflated = puff(raw, &output_room, input, &input_room);
        if (inflated != 0 || output_room != filtered_size) {
            return report("input has invalid PNG image data (inflate returned %d)",
                inflated);
        }
        if (unfilter(raw, width, height) != 0) {
            return report("input uses an unsupported PNG filter");
        }
        /* Re-pack the rows without their filter bytes into plain RGBA. */
        image->pixels = malloc(width * height * RGBA_BYTES);
        if (image->pixels == NULL) {
            return report("out of memory");
        }
        {
            unsigned long row;
            unsigned long stride = width * RGBA_BYTES;

            for (row = 0; row < height; row++) {
                memcpy(image->pixels + row * stride,
                    raw + row * (stride + 1u) + 1u, stride);
            }
        }
    }
    image->width = width;
    image->height = height;
    return 0;
}

static int visible_bounds(const struct image *image, unsigned long *left,
                          unsigned long *top, unsigned long *right,
                          unsigned long *bottom)
{
    unsigned long x;
    unsigned long y;
    int found = 0;

    *left = image->width;
    *top = image->height;
    *right = 0;
    *bottom = 0;
    for (y = 0; y < image->height; y++) {
        for (x = 0; x < image->width; x++) {
            if (image->pixels[(y * image->width + x) * RGBA_BYTES + 3] == 0) {
                continue;
            }
            if (x < *left) {
                *left = x;
            }
            if (y < *top) {
                *top = y;
            }
            if (x + 1 > *right) {
                *right = x + 1;
            }
            if (y + 1 > *bottom) {
                *bottom = y + 1;
            }
            found = 1;
        }
    }
    if (!found) {
        return report("boot logo contains no visible pixels");
    }
    return 0;
}

/* Nearest-neighbour fit onto a white square, matching the previous generator's
 * rounding so the produced header stays comparable. */
static int composite(const struct image *source, unsigned long size,
                     uint32_t *pixels)
{
    unsigned long left;
    unsigned long top;
    unsigned long right;
    unsigned long bottom;
    unsigned long cropped_width;
    unsigned long cropped_height;
    unsigned long scale_base;
    unsigned long output_width;
    unsigned long output_height;
    unsigned long offset_x;
    unsigned long offset_y;
    unsigned long y;
    unsigned long index;
    int status;

    status = visible_bounds(source, &left, &top, &right, &bottom);
    if (status != 0) {
        return status;
    }
    cropped_width = right - left;
    cropped_height = bottom - top;
    scale_base = (cropped_width > cropped_height) ? cropped_width : cropped_height;
    output_width = (unsigned long)((cropped_width * size + scale_base / 2u) / scale_base);
    output_height = (unsigned long)((cropped_height * size + scale_base / 2u) / scale_base);
    if (output_width < 1u) {
        output_width = 1u;
    }
    if (output_height < 1u) {
        output_height = 1u;
    }
    offset_x = (size - output_width) / 2u;
    offset_y = (size - output_height) / 2u;

    for (index = 0; index < size * size; index++) {
        pixels[index] = 0x00ffffffu;
    }
    for (y = 0; y < output_height; y++) {
        unsigned long source_y = top +
            (unsigned long)((y * cropped_height) / output_height);
        unsigned long x;

        if (source_y > bottom - 1u) {
            source_y = bottom - 1u;
        }
        for (x = 0; x < output_width; x++) {
            unsigned long source_x = left +
                (unsigned long)((x * cropped_width) / output_width);
            const unsigned char *sample;
            unsigned int red;
            unsigned int green;
            unsigned int blue;
            unsigned int alpha;
            unsigned int inverse;

            if (source_x > right - 1u) {
                source_x = right - 1u;
            }
            sample = source->pixels + (source_y * source->width + source_x) * RGBA_BYTES;
            red = sample[0];
            green = sample[1];
            blue = sample[2];
            alpha = sample[3];
            inverse = 255u - alpha;
            pixels[(offset_y + y) * size + offset_x + x] =
                (uint32_t)(((red * alpha + 255u * inverse + 127u) / 255u) << 16 |
                ((green * alpha + 255u * inverse + 127u) / 255u) << 8 |
                ((blue * alpha + 255u * inverse + 127u) / 255u));
        }
    }
    return 0;
}

static int emit_header(struct byte_buffer *out, const uint32_t *pixels,
                       unsigned long size)
{
    unsigned long index;
    int status;

    status = appendf(out,
        "/* Generated from logo.png by %s. Do not edit. */\n"
        "#ifndef LEONOS_GENERATED_BOOT_LOGO_H\n"
        "#define LEONOS_GENERATED_BOOT_LOGO_H\n"
        "\n"
        "#include <stdint.h>\n"
        "\n"
        "#define LEONOS_BOOT_LOGO_WIDTH %luu\n"
        "#define LEONOS_BOOT_LOGO_HEIGHT %luu\n"
        "\n"
        "static const uint32_t leonos_boot_logo_pixels[LEONOS_BOOT_LOGO_WIDTH * "
        "LEONOS_BOOT_LOGO_HEIGHT] = {\n",
        TOOL_NAME, size, size) != 0;
    for (index = 0; index < size * size && status == 0; index += 8u) {
        unsigned long column;
        char row[256];
        char *writer = row;
        int written;

        written = snprintf(writer, sizeof(row), "    ");
        if (written < 0) {
            status = -1;
            break;
        }
        writer += (size_t)written;
        for (column = 0; column < 8u && index + column < size * size; column++) {
            written = snprintf(writer, (size_t)(row + sizeof(row) - writer),
                "%s0x00%06lxu", (column == 0) ? "" : ", ",
                (unsigned long)pixels[index + column]);
            if (written < 0 || (size_t)written >= (size_t)(row + sizeof(row) - writer)) {
                status = -1;
                break;
            }
            writer += (size_t)written;
        }
        if (status != 0) {
            break;
        }
        /* The previous generator terminated every row with a comma, including
         * the last one; matching it keeps the headers comparable. */
        written = snprintf(writer, (size_t)(row + sizeof(row) - writer), ",\n");
        if (written < 0 || (size_t)written >= (size_t)(row + sizeof(row) - writer)) {
            status = -1;
            break;
        }
        writer += (size_t)written;
        status = append(out, row, (size_t)(writer - row)) != 0;
    }
    if (status == 0) {
        status = appendf(out, "};\n\n#endif\n") != 0;
    }
    if (status != 0) {
        return report("cannot render the header: %s", strerror(errno));
    }
    return 0;
}

int main(int argc, char **argv)
{
    struct options options;
    struct byte_buffer file = { NULL, 0, 0 };
    struct byte_buffer scratch = { NULL, 0, 0 };
    struct byte_buffer out = { NULL, 0, 0 };
    struct image image = { 0, 0, NULL };
    uint32_t *pixels;
    int status;

    status = parse_options(argc, argv, &options);
    if (status != 0) {
        return status;
    }
    status = read_entire_file(options.input, &file);
    if (status != 0) {
        goto cleanup;
    }
    status = decode_png(file.data, file.len, &image, &scratch);
    if (status != 0) {
        goto cleanup;
    }
    if (options.size > SIZE_MAX / RGBA_BYTES / options.size) {
        status = report("--size is too large");
        goto cleanup;
    }
    pixels = calloc((size_t)options.size * options.size, sizeof(uint32_t));
    if (pixels == NULL) {
        status = report("out of memory");
        goto cleanup;
    }
    status = composite(&image, options.size, pixels);
    if (status == 0) {
        status = emit_header(&out, pixels, options.size);
    }
    free(pixels);
    if (status != 0) {
        goto cleanup;
    }
    if (write_file_if_changed(options.output, out.data, out.len, 0644u) != 0) {
        status = report("cannot publish %s: %s", options.output, strerror(errno));
    }

cleanup:
    free(image.pixels);
    buffer_destroy(&file);
    buffer_destroy(&scratch);
    buffer_destroy(&out);
    return status;
}
