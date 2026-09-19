/* Checked adaptation of the single upstream implementation marker. */
#include "tools/host/common/io.h"
#include <stdio.h>
#include <string.h>
int main(int argc, char **argv) {
  const char *input = NULL, *output = NULL;
  const char marker[] = "#define PORTABLEGL_IMPLEMENTATION";
  const char replacement[] = "/* LeonOS links PortableGL dynamically. */";
  struct byte_buffer data = {0}, result = {0};
  int status = 1;
  for (int i = 1; i < argc; i++) {
    if (!strcmp(argv[i], "--help")) {
      puts("leonos-gears --input gears.c --output gears-upstream.c");
      return 0;
    }
    if (i + 1 == argc)
      goto done;
    if (!strcmp(argv[i], "--input"))
      input = argv[++i];
    else if (!strcmp(argv[i], "--output"))
      output = argv[++i];
    else
      goto done;
  }
  if (!input || !output || read_file_all(input, &data) ||
      data.len > 1024 * 1024 || buffer_reserve(&data, data.len + 1))
    goto done;
  if (memchr(data.data, 0, data.len))
    goto done;
  data.data[data.len] = 0;
  char *found = strstr((char *)data.data, marker);
  if (!found || strstr(found + sizeof(marker) - 1, marker))
    goto done;
  size_t before = (size_t)(found - (char *)data.data),
         after = data.len - before - (sizeof marker - 1);
  size_t length = before + sizeof replacement - 1 + after;
  if (buffer_reserve(&result, length))
    goto done;
  memcpy(result.data, data.data, before);
  memcpy(result.data + before, replacement, sizeof replacement - 1);
  memcpy(result.data + before + sizeof replacement - 1,
         found + sizeof marker - 1, after);
  if (write_file_if_changed(output, result.data, length, 0644))
    goto done;
  status = 0;
done:
  if (status)
    fprintf(stderr, "gears: invalid arguments, input marker, or I/O failure\n");
  buffer_destroy(&data);
  buffer_destroy(&result);
  return status;
}
