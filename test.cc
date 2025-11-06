
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

#include "pico/cyw43_arch.h"
#include "lwip/pbuf.h"
#include "lwip/udp.h"
#include "udp_client_server.h"

// example application
int main() {
  stdio_init_all();
  //disable_power_save();
  if (cyw43_arch_init()) {
    sleep_ms(5000);
    printf("failed to intialize wireless\n");
    while (true) {
      sleep_ms(1000);
    }
  }
  cyw43_arch_enable_sta_mode();
  while (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK, 10000)) {
    sleep_ms(5000);
    printf("failed to connect\n");
  }
  printf("Connect to WIFI SSID: %s\n", WIFI_SSID);
  UDP_Client_Server client;
  client.setup_udp_find_service(123);
  client.find_server();
  printf("found ntp server\n");

  int rf_pin = 21;
  double frequency_Hz = 28126000;
  sleep_ms(3000);
  // set wspr message to my call sign, location, and power
  uint8_t message[] = {3, 3, 2, 2, 2, 2, 2, 2, 3, 0, 2, 0, 3, 1, 1, 0, 0, 0, 1, 2, 2, 1, 2, 1, 1, 1, 3, 0, 0,
    2, 2, 2, 2, 2, 1, 0, 2, 3, 0, 1, 0, 0, 0, 0, 0, 2, 1, 0, 3, 3, 2, 0, 1, 3, 2, 3, 2, 2, 2, 3, 3, 0, 3, 0,
    0, 2, 2, 3, 1, 0, 1, 2, 3, 0, 3, 2, 3, 2, 0, 1, 0, 0, 1, 2, 1, 1, 2, 0, 0, 3, 3, 0, 1, 2, 1, 2, 2, 2, 1,
    0, 0, 0, 0, 2, 3, 0, 0, 3, 0, 0, 1, 3, 1, 2, 3, 3, 0, 2, 1, 3, 0, 1, 0, 0, 2, 3, 1, 1, 2, 2, 2, 0, 0, 3,
    2, 1, 0, 0, 1, 3, 2, 0, 2, 2, 0, 0, 0, 1, 1, 2, 3, 0, 3, 1, 2, 0, 0, 3, 3, 2, 0, 2, 0, 0, 0, 0, 0, 0 };

  printf("message before before object rf_nco is created\n");
  
  for (uint8_t i = 0; i < nco_wspr::number_of_symbols_in_a_message; i++) {
    printf("%d ", message[i]);
  }
  printf("\n");
  nco_wspr rf_nco(rf_pin, frequency_Hz, message);
  
  FreqCountRP2.beginTimer(11, 1000);  // pin 11, 1 second
  sleep_ms(2000);
  uint64_t sum = 0;
  int count = 0;
  uint64_t average = 0;
  printf("starting wspr message transfer\n");
  while (1) {
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
