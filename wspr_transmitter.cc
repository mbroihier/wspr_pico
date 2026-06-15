
#define __STDC_FORMAT_MACROS 1
#include "pico/stdlib.h"
#include <cmath>
#include <malloc.h>
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

// Source - https://stackoverflow.com/q/79222575
// Posted by Richard
// Retrieved 2026-03-05, License - CC BY-SA 4.0

uint32_t getTotalHeap()
{
    extern char __StackLimit, __bss_end__;
    return &__StackLimit - &__bss_end__;
}

uint32_t getFreeHeap()
{
    struct mallinfo m = mallinfo();
    return getTotalHeap() - m.uordblks;
}

// example application
int main() {
  stdio_init_all();
  //disable_power_save();
  sleep_ms(5000);
  printf("Total heap: %u\n", getTotalHeap());
  printf("Free heap: %u\n", getFreeHeap());
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
  printf("Total heap: %u\n", getTotalHeap());
  printf("Free heap: %u\n", getFreeHeap());
  client.setup_udp_find_service(123);
  client.find_server();
  client.setup_udp_client();
  printf("found ntp server\n");
  printf("packetBufferR address in test: %p\n", client.get_packetBufferR_addr());
  uint8_t * ptr = client.get_packetBufferR_addr();
  for (int i = 0; i < 48; i++) {
    printf("%2.2x ", ptr[i]);
    if (i % 16 == 15) printf("\n");
  }

  // get time
  uint8_t packet[] = { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
                       0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
                       0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
  client.send_packet(client.get_remote_ip_addr(), 123, 100, packet, 48);
  // packetBufferR should now have an NTP packet
  uint64_t sample_clock = time_us_64();
  uint8_t seconds_buf[4] = {0};
  for (int i = 0; i < 48; i++) {
    printf("%2.2x ", ptr[i]);
    if (i % 16 == 15) printf("\n");
  }
  memcpy(seconds_buf, client.get_packetBufferR_addr()+40, sizeof(seconds_buf));
  uint32_t seconds_since_1900 = seconds_buf[0] << 24 | seconds_buf[1] << 16 | seconds_buf[2] << 8 |
    seconds_buf[3];
  uint32_t minute = (seconds_since_1900 / 60) % 60;
  uint32_t second = seconds_since_1900 % 60;
  uint32_t first_delay = (60 - second + ((minute & 0x01) == 0) * 60 + 1) * 1000000;  // microseconds until first message
  printf("time received was: %u, minute: %d, second: %d, delay: %d\n", seconds_since_1900, minute, second,
         first_delay);
  uint32_t subsequent_delays = 120 * 1000000;  // every two minutes
  uint64_t send_message_when_time_is_this = sample_clock + first_delay;
  int rf_pin = 21;
  double offset_Hz = 0;
  double tuning_frequency = 28124600;
  double frequency_Hz = tuning_frequency + offset_Hz;
  sleep_ms(3000);
  // set wspr message to my call sign, location, and power
  
  uint8_t message[] = {3, 3, 2, 2, 2, 2, 2, 2, 3, 0, 2, 0, 3, 1, 1, 0, 0, 0, 1, 2, 2, 1, 2, 1, 1, 1, 3, 0, 0,
    2, 2, 2, 2, 2, 1, 0, 2, 3, 0, 1, 0, 0, 0, 0, 0, 2, 1, 0, 3, 3, 2, 0, 1, 3, 2, 3, 2, 2, 2, 3, 3, 0, 3, 0,
    0, 2, 2, 3, 1, 0, 1, 2, 3, 0, 3, 2, 3, 2, 0, 1, 0, 0, 1, 2, 1, 1, 2, 0, 0, 3, 3, 0, 1, 2, 1, 2, 2, 2, 1,
    0, 0, 0, 0, 2, 3, 0, 0, 3, 0, 0, 1, 3, 1, 2, 3, 3, 0, 2, 1, 3, 0, 1, 0, 0, 2, 3, 1, 1, 2, 2, 2, 0, 0, 3,
    2, 1, 0, 0, 1, 3, 2, 0, 2, 2, 0, 0, 0, 1, 1, 2, 3, 0, 3, 1, 2, 0, 0, 3, 3, 2, 0, 2, 0, 0, 0, 0, 0, 0 };
  /*
  uint8_t message[] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
  
  uint8_t message[] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 };
  
  uint8_t message[] = {2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2 };
  
  uint8_t message[] = {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
    3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
    3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
    3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
    3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3 };
  */

  printf("Free heap: %u\n", getFreeHeap());
  nco_wspr * rf_nco;
  double transmission_offset = ((double)rand()/(double)RAND_MAX - 0.5) * 200.0;
  //rf_nco = new nco_wspr(rf_pin, frequency_Hz, message, transmission_offset);
  int freq_index = 0;
  rf_nco = new nco_wspr(rf_pin, message, freq_index);
  printf("Free heap after rf_nco created: %u\n", getFreeHeap());
  
  FreqCountRP2.beginTimer(11, 1000);  // pin 11, 1 second
  sleep_ms(2000);
  uint64_t sum = 0;
  int count = 0;
  uint64_t average = 0;
  while (time_us_64() > send_message_when_time_is_this) {  // advance delay if setup extended past first start time
    send_message_when_time_is_this += subsequent_delays;
  }
  // disable wifi
  cyw43_arch_deinit();
  printf("entering wspr message transfer loop\n");
  while (1) {
    while (time_us_64() < send_message_when_time_is_this) {
      tight_loop_contents();
    }
    send_message_when_time_is_this += subsequent_delays;
    uint64_t start = time_us_64();
    rf_nco->output_wspr_message(false);
    uint64_t stop = time_us_64();
    double delta_time_seconds = (stop - start) / 1000000.0;
    printf("time in output_wspr_message: %lf\n", delta_time_seconds);
    if (FreqCountRP2.available()) {
      uint32_t measured_freq = FreqCountRP2.read();
      sum += measured_freq;
      if (measured_freq > 0) {
        count++;
        average = sum/count;
      }
      printf("measured freq: %d Hz\n", measured_freq);
      printf("average freq: %" PRIu64 " Hz\n", average);
    }
    delete rf_nco;
    freq_index = ++freq_index % 10;
    rf_nco = new nco_wspr(rf_pin, message, freq_index);
    while (time_us_64() >= send_message_when_time_is_this) {
      send_message_when_time_is_this += subsequent_delays;
    }
  }
}
