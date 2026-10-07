/* ---------------------------------------------------------------------- */
/*
 *      build_bit_patterns -- build the bit patterns to produce the WSPR
 *                            frequencies needed
 *
 *      Copyright (C) 2026
 *          Mark Broihier
 *
 */

/* ---------------------------------------------------------------------- */
#include "build_bit_patterns.h"
build_bit_patterns::build_bit_patterns(double frequency_Hz,
                                       double transmission_offset,
                                       double delta) {
  // given a WSPR dial frequency, an offset from that frequency, and the delta between WSPR frequencies,
  // build the bit pattern tables needed to send a WSPR message
  if (delta == 0.0) {
    wspr_delta = 12000.0 / 8192.0;
  } else {
    wspr_delta = delta;
  }
  double phase_shift = 0.0;
  const double TWO_PI = 2.0 * M_PI;
  const double normalized_delta[] = {-1.5 * TWO_PI * wspr_delta / system_clock_frequency,
                                     -0.5 * TWO_PI * wspr_delta / system_clock_frequency,
                                     0.5 * TWO_PI * wspr_delta / system_clock_frequency,
                                     1.5 * TWO_PI * wspr_delta / system_clock_frequency};
  const double center_frequency = TWO_PI * (frequency_Hz + transmission_offset +1500.0);
  const double normalized_center_frequency = center_frequency / system_clock_frequency;
  double integral_sum = 0.0;
  for (uint8_t symbol_type = 0u; symbol_type < number_of_symbol_types; ++symbol_type) {
    uint32_t offset = symbol_type * number_of_words_in_a_DMA_block;
    phase_shift += 1.4;
    integral_sum = 0.0;
    for (uint32_t word = 0; word < number_of_words_in_a_DMA_block; ++word) {
      uint32_t bit_samples = 0;
      for (uint8_t bit = 0; bit < bits_per_word; ++bit) {
        double sample = sin(integral_sum + phase_shift);
        integral_sum += normalized_delta[symbol_type] + normalized_center_frequency;
        while (integral_sum > TWO_PI) integral_sum -= TWO_PI;
        while (integral_sum < - TWO_PI) integral_sum += TWO_PI;
        if (sample > 0) {
          bit_samples |= (1 << bit);
        }
      }
      buffer[offset + word] = bit_samples;
    }
  }
  printf("frequency_Hz:                %f\n", frequency_Hz);
  printf("wspr_delta:                  %f\n", wspr_delta);
  printf("normalized tuning frequency: %f\n", frequency_Hz / system_clock_frequency);
  printf("bit table\n");
  //  for (uint32_t word_index = 0; word_index < number_of_words_in_a_DMA_block;
  //       word_index++) {
  for (uint32_t word_index = 0; word_index < 10;
       word_index++) {
    if (buffer[word_index] == buffer[word_index + number_of_words_in_a_DMA_block] &&
        buffer[word_index] == buffer[word_index + 2*number_of_words_in_a_DMA_block] &&
        buffer[word_index] == buffer[word_index + 3* number_of_words_in_a_DMA_block]) {
      printf("%4.4x %4.4x %4.4x %4.4x\n", buffer[word_index],
             buffer[word_index + number_of_words_in_a_DMA_block],
             buffer[word_index + 2*number_of_words_in_a_DMA_block],
             buffer[word_index + 3*number_of_words_in_a_DMA_block]);
    } else {
      printf("%4.4x %4.4x %4.4x %4.4x*****\n", buffer[word_index],
             buffer[word_index + number_of_words_in_a_DMA_block],
             buffer[word_index + 2*number_of_words_in_a_DMA_block],
             buffer[word_index + 3*number_of_words_in_a_DMA_block]);
    }
  }
}

build_bit_patterns::~build_bit_patterns() {
}

void build_bit_patterns::transfer_bit_pattern_table(uint32_t * external_table) {
  uint32_t * buffer_ptr = buffer;
  for (uint32_t j = 0; j < number_of_symbol_types; j++) {
    for (uint32_t i = 0; i < number_of_words_in_a_DMA_block; i++) {
      *external_table++ = *buffer_ptr++;
    }
  }
}
