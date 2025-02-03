cmake_minimum_required(VERSION 3.13)
#include(pico_sdk_import.cmake)
project(kc1fsz-rc1 C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

#pico_sdk_init()

add_executable(main
  main.cpp
  kc1fsz-tools-cpp/src/rp2040/PicoPollTimer.cpp
  kc1fsz-tools-cpp/src/rp2040/PicoPerfTimer.cpp
)
target_compile_definitions(main PRIVATE PICO_BUILD=1)
#pico_enable_stdio_usb(main 1)
#pico_enable_stdio_uart(main 1)
target_include_directories(main PRIVATE kc1fsz-tools-cpp/include)
target_link_libraries(main pico_stdlib hardware_i2c hardware_clocks hardware_pwm)

# ------ dtmf-test-1 ---------------------------------------------------------
# Target: Host

add_executable(dtmf-test-1
  tests/dtmf-test-1.cpp
  kc1fsz-tools-cpp/src/Common.cpp
#  kc1fsz-tools-cpp/src/AudioAnalyzer.cpp
  kc1fsz-tools-cpp/src/DTMFDetector.cpp
  gsm-0610-codec/src/fixed_math.cpp  
) 
target_compile_options(dtmf-test-1 PRIVATE -fstack-protector-all -Wall -Wpedantic -g)
target_include_directories(dtmf-test-1 PRIVATE kc1fsz-tools-cpp/include)
