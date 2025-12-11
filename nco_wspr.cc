// Based on
//  _  ___  _   _____ _     _
// / |/ _ \/ | |_   _| |__ (_)_ __   __ _ ___
// | | | | | |   | | | '_ \| | '_ \ / _` / __|
// | | |_| | |   | | | | | | | | | | (_| \__ \
// |_|\___/|_|   |_| |_| |_|_|_| |_|\__, |___/
//                                  |___/
//
// Copyright (c) Jonathan P Dawson 2023
// filename: nco.cpp
// description: PIO based NCO for Pi Pico
// License: MIT
//

#include "nco_wspr.h"

void nco_wspr::initialise_waveform_buffer() {
  // Uses floating-point arithmetic and trig functions.
  // Not very fast but doesn't matter because it only runs once.
  wspr_delta = 12000.0 / 8192.0;
  uint64_t time_base = time_us_64();
  uint64_t sample = time_base;
  for (uint8_t symbol_type = 0u; symbol_type < number_of_symbol_types; ++symbol_type) {
    double sample_number = 0.0;
    uint32_t offset = symbol_type * number_of_words_in_a_DMA_block;
    double symbol_frequency = 2.0 * M_PI * (frequency_Hz + symbol_type * wspr_delta + 1500.0);
    double normalized_symbol_frequency = symbol_frequency / system_clock_frequency;
    printf("symbol frequency:            %f\n", symbol_frequency);
    printf("normalized symbol frequency: %10.10f\n", normalized_symbol_frequency);
    for (uint32_t word = 0; word < number_of_words_in_a_DMA_block; ++word) {
      uint32_t bit_samples = 0;
      for (uint8_t bit = 0; bit < bits_per_word; ++bit) {
        double sample = sin(normalized_symbol_frequency * sample_number);
        sample_number += 1.0;
        // could apply dithering here to remove harmonics
        // i.e. sample += (((double)rand()/(double)RANDMAX) - 0.5) * 2.0
        // //random number between -1 and +1
        if (sample > 0) {
          bit_samples |= (1 << bit);
        }
      }
      buffer[offset + word] = bit_samples;
    }
    printf("time delta: %lld\n", time_us_64() - sample);
    sample = time_us_64();
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
  printf("Looking for repeating of pattern\n");
  for (uint32_t word_index = 0; word_index < number_of_words_in_a_DMA_block;
       word_index++) {
    if (buffer[0] == buffer[word_index]) {
      printf("symbol 0 repeats at %d\n", word_index);
    }
    if (buffer[number_of_words_in_a_DMA_block] == buffer[word_index+number_of_words_in_a_DMA_block]) {
      printf("symbol 1 repeats at %d\n", word_index);
    }
    if (buffer[2*number_of_words_in_a_DMA_block] == buffer[word_index+2*number_of_words_in_a_DMA_block]) {
      printf("symbol 2 repeats at %d\n", word_index);
    }
    if (buffer[3*number_of_words_in_a_DMA_block] == buffer[word_index+3*number_of_words_in_a_DMA_block]) {
      printf("symbol 3 repeats at %d\n", word_index);
    }
    if (buffer[1] == buffer[word_index]) {
      printf("symbol 0/w1 repeats at %d\n", word_index);
    }
    if (buffer[number_of_words_in_a_DMA_block+1] == buffer[word_index+number_of_words_in_a_DMA_block]) {
      printf("symbol 1/w1 repeats at %d\n", word_index);
    }
    if (buffer[2*number_of_words_in_a_DMA_block+1] == buffer[word_index+2*number_of_words_in_a_DMA_block]) {
      printf("symbol 2/w1 repeats at %d\n", word_index);
    }
    if (buffer[3*number_of_words_in_a_DMA_block+1] == buffer[word_index+3*number_of_words_in_a_DMA_block]) {
      printf("symbol 3/w1 repeats at %d\n", word_index);
    }
  }
}

nco_wspr::nco_wspr(const uint8_t rf_pin, double frequency_Hz, uint8_t * message) {
  gpio_init(rf_pin);
  gpio_set_dir(rf_pin, GPIO_OUT);
  gpio_set_drive_strength(rf_pin, GPIO_DRIVE_STRENGTH_12MA);
  printf("incoming message\n");
  for (uint8_t i = 0; i < number_of_symbols_in_a_message; i++) {
    printf("%d ", message[i]);
  }
  printf("\n");
  printf("symbols before copy of message\n");
  for (uint8_t i = 0; i < number_of_symbols_in_a_message; i++) {
    printf("%d ", symbols[i]);
  }
  for (int i = 0; i < number_of_symbols_in_a_message; i++) {
    symbols[i] = message[i];
  }
  printf("symbols after copy of message\n");
  for (uint8_t i = 0; i < number_of_symbols_in_a_message; i++) {
    printf("%d ", symbols[i]);
  }
  printf("\n");
  m_rf_pin = rf_pin;
  this->frequency_Hz = frequency_Hz;

  initialise_waveform_buffer();

  // The PIO contains a very simple program that reads a 32-bit word
  // from the FIFO and sends 1 bit per clock to an IO pin
  uint offset = pio_add_program(pio, &stream_bits_program);
  sm = pio_claim_unused_sm(pio, true);
  stream_bits_program_init(pio, sm, offset, rf_pin); // GPIO0

  // 1 transfers the pre-generated blocks of 256-bits to PIO
  // The second DMA configures the first from a table of start addresses
  nco_dma = dma_claim_unused_channel(true);

  // configure DMA from memory to PIO TX FIFO
  nco_dma_cfg = dma_channel_get_default_config(nco_dma);
  channel_config_set_transfer_data_size(&nco_dma_cfg, DMA_SIZE_32);
  channel_config_set_read_increment(&nco_dma_cfg, true);
  channel_config_set_write_increment(&nco_dma_cfg, false);
  channel_config_set_dreq(&nco_dma_cfg, pio_get_dreq(pio, sm, true));
  dma_channel_configure(nco_dma, &nco_dma_cfg,
                        &pio->txf[sm],
                        NULL,
                        10000,  // 10 32 bit transfers
                        false // don't start yet
                        );
  printf("symbols after DMA configuration\n");
  for (uint8_t i = 0; i < nco_wspr::number_of_symbols_in_a_message; i++) {
    printf("%d ", symbols[i]);
  }
  printf("\n");
}

nco_wspr::~nco_wspr() {
  pio_sm_unclaim(pio, sm);
  dma_channel_cleanup(nco_dma);
  dma_channel_unclaim(nco_dma);

  // disable GPIO, pullup/pulldown resistors should be installed
  // to switch off transistors when pin is high impedance
  gpio_deinit(m_rf_pin);
}

// Output a wspr message.
//

void nco_wspr::output_wspr_message() {
  bool first_time = true;
  for (uint32_t symbol = 0u; symbol < number_of_symbols_in_a_message; symbol++) {
    uint32_t * address = &buffer[symbols[symbol] * number_of_words_in_a_DMA_block];  // get the buffer address to use
    for (uint32_t symbol_duration = 0u; symbol_duration < DMA_blocks_per_symbol; symbol_duration++) {  // repeat
      while (dma_channel_is_busy(nco_dma)) {
        tight_loop_contents();
      }
      if (pio->fdebug) {
        //printf("pio stall, potential lost samples debug: %x\n", pio->fdebug);
        pio->fdebug = 0xffffffff; // clear all
      }
      if (symbol_duration != (DMA_blocks_per_symbol - 1)) {  // check for last transfer
        dma_channel_configure(nco_dma, &nco_dma_cfg,
                              &pio->txf[sm],
                              address,
                              10000,  // 10,000 word, 32 bit transfers
                              true // start
                              );
      } else {
        dma_channel_configure(nco_dma, &nco_dma_cfg,
                              &pio->txf[sm],
                              address,
                              6667,  // part of a block 32 bit transfers
                              true // start
                              );
      }        
    }
    printf("m[%d]:%d\n", symbol, symbols[symbol]);
  }
  printf("\n");
}
