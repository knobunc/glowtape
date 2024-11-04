cmake_minimum_required(VERSION 3.25)

# Need to include pico-sdk cmake support. Must happen before project.
include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(glowtape
  LANGUAGES C CXX ASM
  VERSION 1.0
)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 23)
set(PICO_CXX_ENABLE_EXCEPTIONS OFF)
set(PICO_CXX_ENABLE_RTTI OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)  # Get compilation DB

# initialize the Raspberry Pi Pico SDK
pico_sdk_init()

# Tell CMake where to find the executable source file
add_executable(${PROJECT_NAME}
  glowtape.cc
  bdfont-support.c
  font-6x9.c
  font-message.c
  font-timetext.c
  font-large.c
)

# Create map/bin/hex/uf2 files
pico_add_extra_outputs(${PROJECT_NAME})

target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Werror)

# Various pico libraries needed.
target_link_libraries(${PROJECT_NAME}
  pico_stdlib
  pico_printf
  pico_time
  hardware_rtc
  hardware_spi
)

# Choice of available stdio outputs
pico_enable_stdio_usb(${PROJECT_NAME} 1)  # ~13880 extra bytes
pico_enable_stdio_uart(${PROJECT_NAME} 0) # ~1176 extra bytes
