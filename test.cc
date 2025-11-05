
#define __STDC_FORMAT_MACROS 1
#include "pico/stdlib.h"
#include <cmath>
#include <stdio.h>
#include <sys/types.h>  // needed for PRIu64
#include <cinttypes>

#include "hardware/clocks.h"
#include "hardware/gpio.h"
#include "hardware/pll.h"
#include "FreqCountRP2.h"
#include "psu_mode.h"
#include "nco_wspr.h"

// example application
int main() {
  stdio_init_all();
  disable_power_save();

  int rf_pin = 21;
  double frequency_Hz = 28126000;
  //gpio_set_function(rf_pin, GPIO_FUNC_GPCK);
  //clock_gpio_init(rf_pin, CLOCKS_CLK_GPOUT0_CTRL_AUXSRC_VALUE_CLK_USB, 1.0);
  sleep_ms(3000);
  uint8_t message[nco_wspr::number_of_symbols_in_a_message];
  printf("message before before object rf_nco is created\n"); 
  for (uint8_t i = 0; i < nco_wspr::number_of_symbols_in_a_message; i++) {
    message[i] = i < 40 ? 0 : i < 80 ? 1 : i < 120 ? 2 : 3;
  }
  for (uint8_t i = 0; i < nco_wspr::number_of_symbols_in_a_message; i++) {
    printf("%d ", message[i]);
  }
  printf("\n");
  nco_wspr rf_nco(rf_pin, frequency_Hz, message);
  //const double sample_frequency_Hz = 15000;
  //const double sample_frequency_Hz = 10000;
  //const double waveforms_per_sample = rf_nco.get_waveforms_per_sample(sample_frequency_Hz);
  
  FreqCountRP2.beginTimer(11, 1000);  // pin 11, 1 second
  sleep_ms(2000);
  uint64_t sum = 0;
  int count = 0;
  uint64_t average = 0;
  printf("starting wspr message transfer\n");
  while (1) {
    //rf_nco.output_sample(0, waveforms_per_sample);
    uint64_t start = time_us_64();
    rf_nco.output_wspr_message();
    uint64_t stop = time_us_64();
    if (FreqCountRP2.available()) {
      uint32_t measured_freq = FreqCountRP2.read();
      sum += measured_freq;
      if (measured_freq > 0) {
        count++;
        average = sum/count;
      }
      printf("measured freq: %d Hz\n", measured_freq);
      printf("average freq: %" PRIu64 " Hz\n", average);
      double delta_time_seconds = (stop - start) / 1000000.0;
      printf("duration of wspr transmission: %lf\n", delta_time_seconds);
    }
  }
}
