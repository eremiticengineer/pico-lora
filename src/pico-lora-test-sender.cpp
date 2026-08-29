#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/spi.h"

#include <string>
#include <cstring>

#include "FreeRTOS.h"
#include "task.h"

#include "SX1278.hpp"

#define LORA_SEND_TASK_PRIORITY (tskIDLE_PRIORITY + 2UL)

namespace lora_config {
    inline spi_inst_t* SPI = spi0;
    inline constexpr uint SCK   = 18;
    inline constexpr uint MOSI  = 19;
    inline constexpr uint MISO  = 16;
    inline constexpr uint CS    = 17;
    inline constexpr uint RESET = 20;
}

void lora_send_task(void* params) {
    SX1278* pLora = static_cast<SX1278*>(params);

    uint32_t sequence = 0;

    while (true) {
        uint32_t currentSequence = sequence++;

        std::string payload =
            std::to_string(currentSequence) +
            "|" +
            "Hello from Pico";

        if (pLora->send(payload)) {
            printf(
                "TX seq=%lu: %s\n",
                static_cast<unsigned long>(currentSequence),
                payload.c_str()
            );
        }
        else {
            printf(
                "TX failed seq=%lu\n",
                static_cast<unsigned long>(currentSequence)
            );
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

int main( void )
{
    stdio_init_all();

    SX1278Config config;
    config.frequencyHz = 433000000;
    config.bandwidth = LoRaBandwidth::BW_125_KHZ;
    config.codingRate = LoRaCodingRate::CR_4_5;
    config.spreadingFactor = 7;
    config.crcEnabled = true;
    config.preambleLength = 8;
    config.syncWord = 0x12;
    config.txPowerDbm = 17;

    SX1278 lora(
        lora_config::SPI,
        lora_config::CS,
        lora_config::RESET,
        lora_config::SCK,
        lora_config::MOSI,
        lora_config::MISO
    );

    if (lora.init(config)) {
        printf("SX1278 detected, version: 0x%02X\n", lora.getVersion());
        xTaskCreate(lora_send_task, "LoRaSendTask", 512, (void*)&lora, LORA_SEND_TASK_PRIORITY, nullptr);
        vTaskStartScheduler();
    }
    else {
        printf("SX1278 not detected\n");
    }    

    return 0;
}
