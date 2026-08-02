# wspr_pico 


This repository contains C++ code intended for a Raspberry PI Pico W.  It builds a WSPR transmitter for frequencies below 62.5 MHz.  As is, the data are configured to transmit a WSPR message at about 28.126100 MHz.  The message is encoded with my call sign, location, and transmitter power level.  Prior to any use by anyone else, those parameters should be changed.

The Raspberry PI Pico W that this runs on is intended to be a component of a board that I've designed that has additional circuitry that converts the Pico generated square waves into sine waves using a class e amplifier.  The Pico and the board components are powered by a 5v USB connection to the Pico.

Operation:
  - When power is applied to the Pico, it connects to an expected WIFI network (configured during cmake).  On this network, a NTP server is expected.  This server is running on a separate Pico W that is built with the use of the repository https://github.com/mbroihier/time_server_m5.
  - When the transmitter receives time from the server, it determines when the next WSPR window will be and prepares the message for transfer.
  - When the transmitter time is reached, DMA controllers are started that send chains of words to the PIO.  The PIO is programmed to send out the bit pattern in each word.  One bit will be sent every system clock (which is 125 MHz) and this process will continue until a little over 110 seconds of bits are sent.
  - At the end of the message transmission, the transmitter delays until the next WSPR window and then sends the message again at different frequency.
  - Operation terminates when power is shut off.

Parts:
  - Raspberry PI Pico W
  - Computer capable of programming the Pico W
  - WSPR Transmitter3 Board
  - USB power
  - antenna
  - https://github.com/mbroihier/time_server_m5
  - a hotspot that is the host for your WIFI

Software:
  1)  Install the Pico SDK on the development computer
  2)  git clone https://github.com/mbroihier/wspr_pico
  3)  cd wspr_pico
  4)  mkdir build
  5)  cd build
  6)  cmake .. -D WIFI_SSID="your ssid" -D WIFI_PASSWORD="your wifi passwork" -D UDP_PORT="123"
  7)  make
  8)  install wspr_transmitter.uf2 onto the Pico W

