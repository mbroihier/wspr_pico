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
#include "bit_patterns.h"

uint32_t nco_wspr::symbol_dma, nco_wspr::program_symbol_dma, nco_wspr::program_control0,
  nco_wspr::first_time_symbol_dma;
uint32_t nco_wspr::symbol_dma_count, nco_wspr::program_symbol_dma_count, nco_wspr::program_control0_count,
  nco_wspr::first_time_symbol_dma_count;

void nco_wspr::dma_handler() {
  if (dma_hw->intr & 1u << symbol_dma) {
    dma_hw->ints0 = 1u << symbol_dma;  // reset interrupt
    symbol_dma_count++;  // increment count
    // when we get this interrupt, we want program_control0 to run
    dma_start_channel_mask(1u << program_control0);
  }
  if (dma_hw->intr & 1u << program_symbol_dma) {
    dma_hw->ints0 = 1u << program_symbol_dma;  // reset interrupt
    program_symbol_dma_count++;  // increment count
  }
  if (dma_hw->intr & 1u << program_control0) {
    dma_hw->ints0 = 1u << program_control0;
    program_control0_count++;
    // when this happens, trigger first_time_symbol_dma
    if(first_time_symbol_dma_count < number_of_symbols_in_a_message) {
      dma_start_channel_mask(1u << first_time_symbol_dma);
    }
  }
  if (dma_hw->intr & 1u << first_time_symbol_dma) {
    dma_hw->ints0 = 1u << first_time_symbol_dma;
    first_time_symbol_dma_count++;
  }
}

void nco_wspr::setup_control_blocks() {
  // control block 0 contains length, address pairs for building the tone for each symbol
  uint32_t * buffer_ptr = ctr0_block;
  for (uint32_t symbol = 0u; symbol < number_of_symbol_types; symbol++) {
    // get the buffer address to use
    uint32_t * address = &buffer[symbol * number_of_words_in_a_DMA_block] ;
    for (uint32_t i = 1; i < DMA_blocks_per_symbol; i++) {
      *buffer_ptr++ = (i < DMA_blocks_per_symbol - 1) ? 10000 : 6667;
      *buffer_ptr++ = (uint32_t) address;
    }
    *buffer_ptr++ = 0;
    *buffer_ptr++ = 0;
    *buffer_ptr++ = 0;
    *buffer_ptr++ = 0;
  }
  buffer_ptr = ctr0_block;
  if (debug) {
    printf("ctr0_block\n");
    for (uint32_t symbol = 0u; symbol < number_of_symbol_types; symbol++) {
      for (uint32_t i = 0; i < DMA_blocks_per_symbol; i++) {
        printf("%4.4x %4.4x\n", *buffer_ptr++, *buffer_ptr++);
      }
      printf("%4.4x %4.4x\n", *buffer_ptr++, *buffer_ptr++);
    }
  }
  // Control block 1 contains pointers to control block 0 in the order of the symbols
  // necessary to build the WSPR message.  When using this table, the values stored in the table
  // must be used by the destination DMA control block, not the pointer.
  buffer_ptr = ctr1_block;
  for (uint32_t symbol_index = 0; symbol_index < number_of_symbols_in_a_message; symbol_index++) {
    // point to the control block 0 entry to use for generating the full symbol tone
    uint32_t * address = &ctr0_block[symbols[symbol_index] * (DMA_blocks_per_symbol + 1) * 2];
    *buffer_ptr++ = (uint32_t) address;
  }
  *buffer_ptr++ = 0;
  buffer_ptr = ctr1_block;
  if (debug) {
    printf("ctr1_block\n");
    for (uint32_t symbol_index = 0u; symbol_index < number_of_symbols_in_a_message; symbol_index++) {
      uint32_t * contents = (uint32_t *)*buffer_ptr;
      printf("symbol index: %3d: %4.4x %4.4x %4.4x\n", symbol_index, *buffer_ptr++, *contents++, *contents);
    }
    printf("%4.4x\n", *buffer_ptr++);
  }
  // Control block 2 contains information for setting up the first DMA transfer for a symbol
  buffer_ptr = ctr2_block;
  for (uint32_t symbol_index = 0; symbol_index < number_of_symbols_in_a_message; symbol_index++) {
    // point to the control block 0 entry to use for generating the full symbol tone
    uint32_t * address = &buffer[symbols[symbol_index] * number_of_words_in_a_DMA_block];
    *buffer_ptr++ = 10000;  // first block is always 10000
    *buffer_ptr++ = (uint32_t) address;
  }
  *buffer_ptr++ = 0;
  *buffer_ptr++ = 0;
  buffer_ptr = ctr2_block;
  if (debug) {
    printf("ctr2_block\n");
    for (uint32_t symbol_index = 0u; symbol_index < number_of_symbols_in_a_message; symbol_index++) {
      printf("symbol index: %3d: %4.4x %4.4x\n", symbol_index, *buffer_ptr++, *buffer_ptr++);
    }
    printf("%4.4x %4.4x\n", *buffer_ptr++, *buffer_ptr++);
  }
}

void nco_wspr::initialise_waveform_buffer(double delta, double transmission_offset) {
  // Uses floating-point arithmetic and trig functions.
  // Not very fast but doesn't matter because it only runs once.
  uint64_t time_base = time_us_64();
  uint64_t sample = time_base;
  if (delta == 0.0) {
    wspr_delta = 12000.0 / 8192.0;
  } else {
    wspr_delta = delta;
  }
  double phase_shift = 0.0;
  float delta_scale[] = {-1.5, -0.5, 0.5, 1.5};
  for (uint8_t symbol_type = 0u; symbol_type < number_of_symbol_types; ++symbol_type) {
    double sample_number = 0.0;
    uint32_t offset = symbol_type * number_of_words_in_a_DMA_block;
    double symbol_frequency = 2.0 * M_PI * (frequency_Hz + delta_scale[symbol_type] * wspr_delta +
                                            + transmission_offset +1500.0);
    double normalized_symbol_frequency = symbol_frequency / system_clock_frequency;
    printf("symbol frequency:            %f\n", symbol_frequency);
    printf("normalized symbol frequency: %10.10f\n", normalized_symbol_frequency);
    phase_shift += 1.4;
    for (uint32_t word = 0; word < number_of_words_in_a_DMA_block; ++word) {
      uint32_t bit_samples = 0;
      for (uint8_t bit = 0; bit < bits_per_word; ++bit) {
        double sample = sin(normalized_symbol_frequency * sample_number + phase_shift);
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
  setup_control_blocks();
}

void nco_wspr::setup_dma_channels() {
  const uint32_t * address = (uint32_t *) ctr2_block[1];  // first symbol DMA source address
  // create a DMA channel that will DMA the symbol bit patterns to the PIO
  symbol_dma = dma_claim_unused_channel(true);
  symbol_dma_cfg = dma_channel_get_default_config(symbol_dma);
  channel_config_set_transfer_data_size(&symbol_dma_cfg, DMA_SIZE_32);
  channel_config_set_read_increment(&symbol_dma_cfg, true);
  channel_config_set_write_increment(&symbol_dma_cfg, false);
  channel_config_set_dreq(&symbol_dma_cfg, pio_get_dreq(pio, sm, true));
  dma_channel_configure(symbol_dma, &symbol_dma_cfg,
                        &pio->txf[sm],
                        address, // first control block for data transfer to PIO
                        10000,   // 10 32 bit transfers
                        false // don't start yet
                        );
  symbol_dma_count = 0;
  dma_channel_set_irq0_enabled(symbol_dma, true);
  irq_set_exclusive_handler(DMA_IRQ_0, dma_handler);
  irq_set_enabled(DMA_IRQ_0, true);
  // this DMA control setup triggers symbol_dma to run a total of 267 times (given that the first pair is skipped)
  //address = &ctr0_block[symbols[0] * (DMA_blocks_per_symbol + 1)*2] + 2;
  address = &ctr0_block[symbols[0] * (DMA_blocks_per_symbol + 1)*2];
  program_symbol_dma = dma_claim_unused_channel(true);
  program_symbol_dma_cfg = dma_channel_get_default_config(program_symbol_dma);
  channel_config_set_transfer_data_size(&program_symbol_dma_cfg, DMA_SIZE_32);
  channel_config_set_read_increment(&program_symbol_dma_cfg, true);
  channel_config_set_write_increment(&program_symbol_dma_cfg, true);
  channel_config_set_ring(&program_symbol_dma_cfg, true, 3); // byte boundary on write ptr
  dma_channel_configure(program_symbol_dma, &program_symbol_dma_cfg,
                        &dma_hw->ch[symbol_dma].al3_transfer_count,  // write to symbol_dma transfer count then source
                        address, // symbol_dma next data for first symbol
                        2,       // 2 32 bit transfers
                        false // don't start yet
                        );
  // address points to a block that is 266 pairs (size, tone ptr) long with zeros at the end, when zeros are hit,
  // this will trigger the next symbol
  
  channel_config_set_chain_to(&symbol_dma_cfg, program_symbol_dma);  // when 10,000 words have been transferred,
                                                                 // go to next transfer
  channel_config_set_irq_quiet(&symbol_dma_cfg, true);
  program_symbol_dma_count = 0;
  dma_channel_set_irq0_enabled(program_symbol_dma, true);
  //channel_config_set_irq_quiet(&program_symbol_dma_cfg, true);

  // now we need DMA transfers to setup for the next symbol by programming program_symbol_dma and symbol_dma
  program_control0_count = 0;
  address = &ctr1_block[1] ;  // for first execution, index into the second message symbol
  program_control0 = dma_claim_unused_channel(true);
  program_control0_cfg = dma_channel_get_default_config(program_control0);
  channel_config_set_transfer_data_size(&program_control0_cfg, DMA_SIZE_32);
  channel_config_set_read_increment(&program_control0_cfg, true);
  channel_config_set_write_increment(&program_control0_cfg, false);
  dma_channel_configure(program_control0, &program_control0_cfg,
                        &dma_hw->ch[program_symbol_dma].read_addr, // write to program_symbol_dma read address
                        address, // next tone to send
                        1,       // 1 32 bit transfers
                        false // don't start yet
                        );
  dma_channel_set_irq0_enabled(program_control0, true);
  // now we need to reprogram symbol_dma and retrigger it to start the next tone
  first_time_symbol_dma_count = 0;
  address = &ctr2_block[1*2]; // each entry is 2 words long, point to the second entry (1)
  first_time_symbol_dma = dma_claim_unused_channel(true);
  first_time_symbol_dma_cfg = dma_channel_get_default_config(first_time_symbol_dma);
  channel_config_set_transfer_data_size(&first_time_symbol_dma_cfg, DMA_SIZE_32);
  channel_config_set_read_increment(&first_time_symbol_dma_cfg, true);
  channel_config_set_write_increment(&first_time_symbol_dma_cfg, true);
  channel_config_set_ring(&first_time_symbol_dma_cfg, true, 3); // byte boundary on write ptr
  dma_channel_configure(first_time_symbol_dma, &first_time_symbol_dma_cfg,
                        &dma_hw->ch[symbol_dma].al3_transfer_count,  // write to control transfer count then source
                        address,  // symbol_dma next data for first symbol
                        2,                        // 2 32 bit transfers
                        false // don't start yet
                        );
  
  dma_channel_set_irq0_enabled(first_time_symbol_dma, true);
}
nco_wspr::nco_wspr(const uint8_t rf_pin, uint8_t * message, uint32_t buffer_select) {

  use_table = true;
  gpio_init(rf_pin);
  gpio_set_dir(rf_pin, GPIO_OUT);
  gpio_set_drive_strength(rf_pin, GPIO_DRIVE_STRENGTH_12MA);
  m_rf_pin = rf_pin;

  for (int i = 0; i < number_of_symbols_in_a_message; i++) {
    symbols[i] = message[i];
  }
  
  // The PIO contains a very simple program that reads a 32-bit word
  // from the FIFO and sends 1 bit per clock to an IO pin
  uint offset = pio_add_program(pio, &stream_bits_program);
  sm = pio_claim_unused_sm(pio, true);
  stream_bits_program_init(pio, sm, offset, rf_pin); // GPIO0

  // nco_dma transfers the pre-generated blocks to PIO and chains to intra_symbol_dma when it completes
  // intra_symbol_dma programs nco_dma for the duration of a symbol and chains to end_symbol_dma
  // end_symbol_dma programs nco_dma for the final, shorter, DMA block for the end of the symbol
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
  // initialize RAM buffer using a FLASH entry
  uint32_t * buffer_ptr = buffer;
  const uint32_t * source_ptr = &bit_pattern_table[buffer_select *  number_of_words_in_a_DMA_block *
                                                   number_of_symbol_types];
  for (int k = 0; k < number_of_words_in_a_DMA_block * number_of_symbol_types; k++) {
    *buffer_ptr++ = *source_ptr++;
  }
  setup_control_blocks();
  setup_dma_channels();
}
nco_wspr::nco_wspr(const uint8_t rf_pin, double frequency_Hz, uint8_t * message, double transmission_offset,
                   double delta) {
  gpio_init(rf_pin);
  gpio_set_dir(rf_pin, GPIO_OUT);
  gpio_set_drive_strength(rf_pin, GPIO_DRIVE_STRENGTH_12MA);
  printf("incoming message\n");
  for (uint8_t i = 0; i < number_of_symbols_in_a_message; i++) {
    printf("%d ", message[i]);
  }
  printf("\n");
  m_rf_pin = rf_pin;
  this->frequency_Hz = frequency_Hz;

  initialise_waveform_buffer(delta, transmission_offset);

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
}

void nco_wspr::tear_down_dma() {
  dma_channel_abort(symbol_dma);
  dma_channel_abort(program_symbol_dma);
  dma_channel_abort(program_control0);
  dma_channel_abort(first_time_symbol_dma);
  dma_channel_cleanup(symbol_dma);
  dma_channel_unclaim(symbol_dma);
  dma_channel_cleanup(program_symbol_dma);
  dma_channel_unclaim(program_symbol_dma);
  dma_channel_cleanup(program_control0);
  dma_channel_unclaim(program_control0);
  dma_channel_cleanup(first_time_symbol_dma);
  dma_channel_unclaim(first_time_symbol_dma);
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

void nco_wspr::output_wspr_message(bool default_sending_mode) {
  if (default_sending_mode) {
    for (uint32_t symbol = 0u; symbol < number_of_symbols_in_a_message; symbol++) {
      // get the buffer address to use
      const uint32_t * address = &buffer[symbols[symbol] * number_of_words_in_a_DMA_block] ;
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
  } else {
    // start first DMA transfer, this block chains to another starting a chain of DMA processes that perform the
    // transfer of the entire message
    const uint32_t * address = &buffer[symbols[0] * number_of_words_in_a_DMA_block] ;
    uint64_t start_time = time_us_64();
    dma_channel_configure(symbol_dma, &symbol_dma_cfg,
                          &pio->txf[sm],
                          address,
                          10000,  // 10,000 word, 32 bit transfers
                          true // start
                          );

    uint32_t not_busy_count = 0;
    while (not_busy_count < 6) {
      bool dma_busy = dma_channel_is_busy(symbol_dma) || dma_channel_is_busy(program_symbol_dma) ||
        dma_channel_is_busy(program_control0) || dma_channel_is_busy(first_time_symbol_dma);
      if (dma_busy) {
        not_busy_count = 0;
      } else {
        not_busy_count++;
      }                  
      if (first_time_symbol_dma_count == 162) {
        printf(" %d symbol_dma\n", dma_channel_is_busy(symbol_dma));
        printf(" %d program_symbol_dma\n", dma_channel_is_busy(program_symbol_dma));
        printf(" %d program_control0\n", dma_channel_is_busy(program_control0));
        printf(" %d first_time_symbol_dma\n", dma_channel_is_busy(first_time_symbol_dma));
        break;
      }
      //tight_loop_contents();
    }
    uint64_t end_time = time_us_64();
    printf("transmission time: %" PRIu64 " micro seconds\n", end_time - start_time);
    printf("symbol_dma interrupted %u times\n", symbol_dma_count);
    printf("program_symbol_dma interrupted %u times\n", program_symbol_dma_count);
    printf("program_control0 interrupted %u times\n", program_control0_count);
    printf("first_time_symbol_dma interrupt %u times\n", first_time_symbol_dma_count);
    if (pio->fdebug) {
      //printf("pio stall, potential lost samples debug: %x\n", pio->fdebug);
      pio->fdebug = 0xffffffff; // clear all
    }
    tear_down_dma();
    setup_dma_channels();
  }
}
