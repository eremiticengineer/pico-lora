cmake_minimum_required(VERSION 3.13)

set(PROJECT pico_lora_test)
set(PROJECT_SENDER pico_lora_test_sender)
set(PROJECT_RECEIVER pico_lora_test_receiver)

set(PICO_BOARD pico2 CACHE STRING "Board type")

include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(${PROJECT} C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 23)

pico_sdk_init()

message(STATUS "PICO_PLATFORM=${PICO_PLATFORM}")

set(FREERTOS_KERNEL_PATH
    ${CMAKE_CURRENT_LIST_DIR}/lib/FreeRTOS-Kernel
)

include(
    ${FREERTOS_KERNEL_PATH}/portable/ThirdParty/GCC/RP2040/FreeRTOS_Kernel_import.cmake
)

# ---------------------------------------------------------
# LoRa sender
# ---------------------------------------------------------
add_executable(${PROJECT_SENDER}
  src/pico-lora-test-sender.cpp
  src/SX1278/SX1278.cpp
)

target_include_directories(${PROJECT_SENDER} PUBLIC
  include/
  src/SX1278
)

target_link_libraries(${PROJECT_SENDER}
    pico_stdlib
    hardware_spi
    FreeRTOS-Kernel-Heap4
)

pico_enable_stdio_usb(${PROJECT_SENDER} 1)
pico_enable_stdio_uart(${PROJECT_SENDER} 0)
pico_add_extra_outputs(${PROJECT_SENDER})

# ---------------------------------------------------------
# LoRa receiver
# ---------------------------------------------------------
add_executable(${PROJECT_RECEIVER}
  src/pico-lora-test-receiver.cpp
  src/SX1278/SX1278.cpp
)

target_include_directories(${PROJECT_RECEIVER} PUBLIC
  include/
  src/SX1278
)

target_link_libraries(${PROJECT_RECEIVER}
    pico_stdlib
    hardware_spi
    FreeRTOS-Kernel-Heap4
)

pico_enable_stdio_usb(${PROJECT_RECEIVER} 1)
pico_enable_stdio_uart(${PROJECT_RECEIVER} 0)
pico_add_extra_outputs(${PROJECT_RECEIVER})

