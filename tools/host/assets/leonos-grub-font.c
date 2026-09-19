#define _POSIX_C_SOURCE 200809L
#include "tools/host/common/io.h"
#include <stdio.h>
#include <stdlib.h>
int main(int argc, char **argv) {
  if (argc != 3) {
    fprintf(stderr, "usage: leonos-grub-font PSF OUTPUT\n");
    return 2;
  }
  struct byte_buffer input = {0};
  if (read_file_all(argv[1], &input)) {
    perror(argv[1]);
    return 1;
  }
  if (input.len < 4100 || input.data[0] != 0x36 || input.data[1] != 4 ||
      input.data[3] != 16) {
    fprintf(stderr, "invalid PSF font\n");
    buffer_destroy(&input);
    return 1;
  }
  char *data = NULL;
  size_t size = 0;
  FILE *out = open_memstream(&data, &size);
  if (!out) {
    buffer_destroy(&input);
    return 1;
  }
  fputs(
      "STARTFONT 2.1\nFONT "
      "-LeonOS-Pixel-Medium-R-Normal--16-160-75-75-C-80-ISO10646-1\nSIZE 16 75 "
      "75\nFONTBOUNDINGBOX 8 16 0 0\nSTARTPROPERTIES 15\nFOUNDRY "
      "\"LeonOS\"\nFONT_ASCENT 16\nFONT_DESCENT 0\nFAMILY_NAME \"LeonOS "
      "Pixel\"\nWEIGHT_NAME \"Regular\"\nSLANT \"R\"\nSETWIDTH_NAME "
      "\"Normal\"\nPIXEL_SIZE 16\nPOINT_SIZE 160\nRESOLUTION_X "
      "75\nRESOLUTION_Y 75\nSPACING \"C\"\nAVERAGE_WIDTH 80\nCHARSET_REGISTRY "
      "\"ISO10646\"\nCHARSET_ENCODING \"1\"\nENDPROPERTIES\nCHARS 95\n",
      out);
  for (unsigned cp = 32; cp <= 126; ++cp) {
    if (cp == 32)
      fputs("STARTCHAR space\n", out);
    else if (cp == 126)
      fputs("STARTCHAR asciitilde\n", out);
    else
      fprintf(out, "STARTCHAR uni%04X\n", cp);
    fprintf(out,
            "ENCODING %u\nSWIDTH 500 0\nDWIDTH 8 0\nBBX 8 16 0 0\nBITMAP\n",
            cp);
    for (unsigned y = 0; y < 16; ++y)
      fprintf(out, "%02X\n", input.data[4 + cp * 16 + y]);
    fputs("ENDCHAR\n", out);
  }
  fputs("ENDFONT\n", out);
  int bad = ferror(out);
  if (fclose(out))
    bad = 1;
  if (!bad && write_file_if_changed(argv[2], data, size, 0644))
    bad = 1;
  free(data);
  buffer_destroy(&input);
  return bad ? 1 : 0;
}
