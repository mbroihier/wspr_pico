//  _  ___  _   _____ _     _
// / |/ _ \/ | |_   _| |__ (_)_ __   __ _ ___
// | | | | | |   | | | '_ \| | '_ \ / _` / __|
// | | |_| | |   | | | | | | | | | | (_| \__ \
// |_|\___/|_|   |_| |_| |_|_|_| |_|\__, |___/
//                                  |___/
//
// Copyright (c) Jonathan P Dawson 2023
// filename: nco.h
// description: PIO based NCO for Pi Pico
// License: MIT
//

#ifndef NCO_H__
#define NCO_H__

#include "hardware/dma.h"
#include "hardware/pio.h"
#include "hardware/sync.h"
#include "pico/stdlib.h"
#include "stream_bits.pio.h"
#include <cmath>
#include <stdio.h>

class nco_wspr {
public:
  static const uint32_t number_of_symbols_in_a_message = 162u;
private:
  uint8_t m_rf_pin;
  PIO pio = pio0;
  uint32_t nco_dma, chain_dma, sm;
  dma_channel_config nco_dma_cfg;
  dma_channel_config chain_dma_cfg;
  static const uint32_t bits_per_word = 32u;
  static const uint32_t words_per_symbol =      2667u;
  static const uint32_t DMA_blocks_per_symbol = 267u;
  static const uint32_t number_of_symbol_types = 4u;
  static const uint32_t number_of_words_in_a_DMA_block = 10000u;
  static const uint64_t system_clock_frequency = 125000000u;
  double frequency_Hz;
  double wspr_delta;
  uint32_t buffer[number_of_symbol_types * number_of_words_in_a_DMA_block]
      __attribute__((aligned(4)));
  uint8_t symbols[number_of_symbols_in_a_message];
  void initialise_waveform_buffer(double delta, double transmission_offset);
  bool use_table = false;

public:
  nco_wspr(const uint8_t rf_pin, uint8_t * message, uint32_t buffer_select);
  nco_wspr(const uint8_t rf_pin, double frequency_Hz, uint8_t * message, double transmission_offset, double delta=0.0);
  ~nco_wspr();
  void output_wspr_message();
};

#endif
