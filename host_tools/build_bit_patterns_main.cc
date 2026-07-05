#include "build_bit_patterns.h"
int main (int argc, char *argv[]) {
  double freq, offset, delta;
  offset = 0.0;
  if (argc == 1) {
    freq = 28124600.0;
    delta = 0.0;  // this will force the default
  } else {
    sscanf(argv[1], "%lf", &freq);
    sscanf(argv[2], "%lf", &delta);
  }
  build_bit_patterns object(freq, offset, delta);
  uint32_t buffer[40000];
  object.transfer_bit_pattern_table(buffer);
  FILE * bit_pattern_file = fopen("bit_pattern_file.txt", "a");
  for (int i = 0; i < 40000; i++) {
    fprintf(bit_pattern_file, "%u, ", buffer[i]);
    if (i % 10 == 9) {
      fprintf(bit_pattern_file, "\n");
    }
  }
  fclose(bit_pattern_file);
}
  
    
