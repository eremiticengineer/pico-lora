cmake_minimum_required(VERSION 3.13)

set(LIBRARY pico_lora)
set(PROJECT_SENDER pico_lora_test_sender)
set(PROJECT_RECEIVER pico_lora_test_receiver)

include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(${PROJECT_SENDER} C CXX ASM)
project(${PROJECT_RECEIVER} C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 23)

pico_sdk_init()

message(STATUS "DEVICE=${DEVICE}")
message(STATUS "PICO_BOARD=${PICO_BOARD}")
message(STATUS "PICO_PLATFORM=${PICO_PLATFORM}")

# ---------------------------------------------------------
# Library
# ---------------------------------------------------------
add_library(${LIBRARY}
    src/SX1278.cpp
)

target_include_directories(${LIBRARY} PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/src
)

target_link_libraries(${LIBRARY} PUBLIC
    pico_stdlib
    hardware_spi
)

# ---------------------------------------------------------
# Standalone tests
# ---------------------------------------------------------

# ---------------------------------------------------------
# LoRa sender
# ---------------------------------------------------------
if(PROJECT_IS_TOP_LEVEL)
    if(DEVICE STREQUAL "seeed_xiao_rp2040")
        set(PICO_BOARD pico CACHE STRING "Board type" FORCE)
    elseif(DEVICE STREQUAL "pico2")
        set(PICO_BOARD pico2 CACHE STRING "Board type" FORCE)
    else()
        message(FATAL_ERROR "Unknown DEVICE='${DEVICE}'. Use pico2 or xiao.")
    endif()

    set(FREERTOS_KERNEL_PATH
        ${CMAKE_CURRENT_LIST_DIR}/lib/FreeRTOS-Kernel
    )

    include(
        ${FREERTOS_KERNEL_PATH}/portable/ThirdParty/GCC/RP2040/FreeRTOS_Kernel_import.cmake
    )

    add_executable(${PROJECT_SENDER}
        examples/pico-lora-test-sender.cpp
    )

    target_include_directories(${PROJECT_SENDER} PUBLIC
        include/
    )

    target_compile_definitions(${PROJECT_SENDER} PRIVATE
        DEVICE_${DEVICE}
    )

    target_link_libraries(${PROJECT_SENDER}
        pico_stdlib
        FreeRTOS-Kernel-Heap4
        ${LIBRARY}
    )

    pico_enable_stdio_usb(${PROJECT_SENDER} 1)
    pico_enable_stdio_uart(${PROJECT_SENDER} 0)
    pico_add_extra_outputs(${PROJECT_SENDER})

    # ---------------------------------------------------------
    # LoRa receiver
    # ---------------------------------------------------------
    add_executable(${PROJECT_RECEIVER}
        examples/pico-lora-test-receiver.cpp
    )

    target_include_directories(${PROJECT_RECEIVER} PUBLIC
        include/
    )

    target_compile_definitions(${PROJECT_RECEIVER} PRIVATE
        DEVICE_${DEVICE}
    )

    target_link_libraries(${PROJECT_RECEIVER}
        pico_stdlib
        FreeRTOS-Kernel-Heap4
        ${LIBRARY}
    )

    pico_enable_stdio_usb(${PROJECT_RECEIVER} 1)
    pico_enable_stdio_uart(${PROJECT_RECEIVER} 0)
    pico_add_extra_outputs(${PROJECT_RECEIVER})
endif()