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
#include <sys/types.h>  // needed for PRIu64
#include <cinttypes>
#include <cmath>
#include <stdio.h>

class nco_wspr {
public:
  static const uint32_t number_of_symbol_types = 4u;
  static const uint32_t number_of_symbols_in_a_message = 162u;
  static uint32_t symbol_dma, program_symbol_dma, program_control0, first_time_symbol_dma;  // DMA channel numbers
  static uint32_t symbol_dma_count, program_symbol_dma_count, program_control0_count, first_time_symbol_dma_count;
  static int32_t blocks[number_of_symbol_types], last[number_of_symbol_types], last_last[number_of_symbol_types];
  static uint32_t * ctr0_starts[number_of_symbol_types];

private:
  bool debug = true;
  uint8_t m_rf_pin;
  PIO pio = pio0;
  uint32_t nco_dma, chain_dma, sm;
  dma_channel_config nco_dma_cfg;
  dma_channel_config chain_dma_cfg;
  static const uint32_t bits_per_word = 32u;
  static const uint32_t DMA_blocks_per_symbol = 267u + 30u; //add 30 extra blocks
  static const uint32_t words_per_symbol = 266*10000u + 6667u; //words per symbol - about .68 seconds
  static const uint32_t number_of_words_in_a_DMA_block = 10000u;
  static const uint64_t system_clock_frequency = 125000000u;

  double frequency_Hz;
  double wspr_delta;
  uint32_t buffer[number_of_symbol_types * number_of_words_in_a_DMA_block]
      __attribute__((aligned(4)));
  uint8_t symbols[number_of_symbols_in_a_message];
  
  // DMA channel numbers and configurations
  dma_channel_config symbol_dma_cfg, program_symbol_dma_cfg, program_control0_cfg, first_time_symbol_dma_cfg;
  // DMA control block data
  uint32_t ctr0_block[(DMA_blocks_per_symbol * 2 + 2) * number_of_symbol_types];
  uint32_t ctr1_block[number_of_symbols_in_a_message + 1];
  uint32_t ctr2_block[number_of_symbols_in_a_message * 2 + 2];
  void setup_control_blocks();
  
  void initialise_waveform_buffer(double delta, double transmission_offset);
  bool use_table = false;
  static void dma_handler();

public:
  nco_wspr(const uint8_t rf_pin, uint8_t * message, uint32_t buffer_select);
  nco_wspr(const uint8_t rf_pin, double frequency_Hz, uint8_t * message, double transmission_offset, double delta=0.0);
  ~nco_wspr();
  void tear_down_dma();
  void setup_dma_channels();
  void output_wspr_message(bool default_sending_mode = true);
};

#endif
