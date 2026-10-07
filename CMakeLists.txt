cmake_minimum_required(VERSION 3.12)
set(PICO_BOARD "pico_w")
set(CMAKE_EXE_LINKER_FLAGS "-Wl,--print-memory-usage")
include("$ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake")
set(PROJECT "hello_pio")
project(${PROJECT} C CXX ASM)
pico_sdk_init()
add_executable(hello_pio)

pico_generate_pio_header(hello_pio ${CMAKE_CURRENT_LIST_DIR}/hello.pio)

target_sources(hello_pio PRIVATE hello.c)

target_link_libraries(hello_pio PRIVATE
        pico_stdlib
        hardware_pio
        )

# Pass cmake -DHELLO_PIO_LED_PIN=x, where x is the pin you want to use
if(HELLO_PIO_LED_PIN)
        target_compile_definitions(hello_pio PRIVATE
                HELLO_PIO_LED_PIN=${HELLO_PIO_LED_PIN}
        )
endif()

pico_add_extra_outputs(hello_pio)
pico_enable_stdio_usb(${PROJECT} 0)
pico_enable_stdio_uart(${PROJECT} 1)

set(PROJECT2 "hello_48MHz")
project(${PROJECT2} C CXX ASM)

add_executable(${PROJECT2})

target_sources(${PROJECT2} PRIVATE ${PROJECT2}.c)

target_link_libraries(${PROJECT2} PRIVATE
        pico_stdlib
        )

pico_add_extra_outputs(${PROJECT2})
pico_enable_stdio_usb(${PROJECT2} 0)
pico_enable_stdio_uart(${PROJECT2} 1)

set(PROJECT3 "hello_gpout")
project(${PROJECT2} C CXX ASM)

add_executable(${PROJECT3})

target_sources(${PROJECT3} PRIVATE ${PROJECT3}.c)

target_link_libraries(${PROJECT3} PRIVATE
        pico_stdlib
        )

pico_add_extra_outputs(${PROJECT3})
pico_enable_stdio_usb(${PROJECT3} 0)
pico_enable_stdio_uart(${PROJECT3} 1)

set(PROJECT4 "transmitter_serial_example")
project(${PROJECT4} C CXX ASM)

add_executable(${PROJECT4})
pico_generate_pio_header(${PROJECT4} ${CMAKE_CURRENT_LIST_DIR}/stream_bits.pio)

target_include_directories(${PROJECT4} PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
        ${CMAKE_CURRENT_LIST_DIR}/.. # for our common lwipopts
)

target_sources(${PROJECT4} PRIVATE
  transmitter_serial_example.cc
  transmit.cc
  modulator.cc
  adc.cc
  nco.cc
  psu_mode.cc
  pwm.cc
  FreqCountRP2.cc
  signal_generator.cc
  half_band_filter.cc
  half_band_filter2.cc
  cordic.cc
  )

target_link_libraries(${PROJECT4} PRIVATE
        pico_stdlib
        hardware_pio
        hardware_dma
        hardware_pwm
        hardware_adc
        pico_cyw43_arch_lwip_poll
        pico_cyw43_driver
        )

pico_add_extra_outputs(${PROJECT4})
pico_enable_stdio_usb(${PROJECT4} 0)
pico_enable_stdio_uart(${PROJECT4} 1)

set(PROJECT5 "wspr_transmitter")
project(${PROJECT5} C CXX ASM)

add_executable(${PROJECT5})

target_compile_definitions(${PROJECT5} PRIVATE
        WIFI_SSID=\"${WIFI_SSID}\"
        WIFI_PASSWORD=\"${WIFI_PASSWORD}\"
        UDP_PORT=${UDP_PORT}
        )

pico_generate_pio_header(${PROJECT5} ${CMAKE_CURRENT_LIST_DIR}/stream_bits.pio)

target_include_directories(${PROJECT5} PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
        ${CMAKE_CURRENT_LIST_DIR}/.. # for our common lwipopts
)

target_sources(${PROJECT5} PRIVATE
  ${PROJECT5}.cc
  FreqCountRP2.cc
  psu_mode.cc
  nco.cc
  nco_wspr.cc
  udp_client_server.cc
  dnsserver.c
  dhcpserver.c
  )

target_link_libraries(${PROJECT5} PRIVATE
        pico_stdlib
        hardware_pio
        hardware_dma
        hardware_pwm
        hardware_rtc
        pico_cyw43_arch_lwip_poll
        pico_cyw43_driver
        )

pico_add_extra_outputs(${PROJECT5})
pico_enable_stdio_usb(${PROJECT5} 1)
pico_enable_stdio_uart(${PROJECT5} 0)
# host compiles for host tools
include(ExternalProject)
set(HOST_PROJECT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/host_tools")
ExternalProject_Add(host_tool_target
        SOURCE_DIR "${HOST_PROJECT_DIR}"
        # INCLUDE_DIR "${HOST_PROJECT_DIR}"
        CMAKE_ARGS
                -DCMAKE_TOOLCHAIN_FILE=
                -DCMAKE_BUILD_TYPE=Release
        CMAKE_CACHE_ARGS
                -DCMAKE_RUNTIME_OUTPUT_DIRCTORY:PATH=${CMAKE_BINARY_DIR}
        INSTALL_COMMAND ""
        )
