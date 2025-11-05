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
  for (uint8_t symbol_type = 0u; symbol_type < number_of_symbol_types; ++symbol_type) {
    uint32_t sample_number = 0u;
    uint32_t offset = symbol_type * number_of_words_in_a_DMA_block;
    double normalized_symbol_frequency = normalized_frequency + symbol_type * wspr_delta / system_clock_frequency;
    for (uint32_t word = 0; word < number_of_words_in_a_DMA_block; ++word) {
      uint32_t bit_samples = 0;
      for (uint8_t bit = 0; bit < bits_per_word; ++bit) {
        double sample = sin(2 * M_PI * normalized_symbol_frequency * sample_number++);
        // could apply dithering here to remove harmonics
        // i.e. sample += (((double)rand()/(double)RANDMAX) - 0.5) * 2.0
        // //random number between -1 and +1
        if (sample > 0) {
          bit_samples |= (1 << bit);
        }
      }
      buffer[offset + word] = bit_samples;
    }
  }
}

nco_wspr::nco_wspr(const uint8_t rf_pin, double frequency_Hz, uint8_t * message) {
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
  normalized_frequency = frequency_Hz/system_clock_frequency;
  wspr_delta = 1.46;  // frequency delta between wspr symbols
  m_rf_pin = rf_pin;

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
                        10,  // 10 32 bit transfers
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
      dma_channel_configure(nco_dma, &nco_dma_cfg,
                            &pio->txf[sm],
                            address,
                            10,  // 10 32 bit transfers
                            true // start
                            );
    }
    printf("m[%d]:%d\n", symbol, symbols[symbol]);
  }
  printf("\n");
}
