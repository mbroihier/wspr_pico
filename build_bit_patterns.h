// Build the WSPR frequency bit patterns 
//

#ifndef BUILD_BIT_PATTERNS_H__
#define BUILD_BIT_PATTERNS_H__
#include <cinttypes>
#include <cmath>
#include <stdio.h>

class build_bit_patterns {
private:
  bool debug = true;

  static const uint32_t bits_per_word = 32u;
  static const uint32_t number_of_symbol_types = 4u;
  static const uint32_t number_of_words_in_a_DMA_block = 10000u;
  static const uint64_t system_clock_frequency = 125000000u;

  double frequency_Hz;
  double wspr_delta;
  uint32_t buffer[number_of_symbol_types * number_of_words_in_a_DMA_block];

public:
  build_bit_patterns(double frequency_Hz, double transmission_offset, double delta=0.0);
  ~build_bit_patterns();
  void transfer_bit_pattern_table(uint32_t * external_table);
};

#endif
// BUILD_BIT_PATTERNS_H__
