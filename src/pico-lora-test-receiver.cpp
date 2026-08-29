#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/spi.h"

#include "FreeRTOS.h"
#include "task.h"

#include "SX1278.hpp"

#define LORA_RECEIVE_TASK_PRIORITY (tskIDLE_PRIORITY + 2UL)

namespace lora_config {
    inline spi_inst_t* SPI = spi0;
    inline constexpr uint SCK   = 18;
    inline constexpr uint MOSI  = 19;
    inline constexpr uint MISO  = 16;
    inline constexpr uint CS    = 17;
    inline constexpr uint RESET = 20;
}

void lora_receive_task(void* params) {
    SX1278* pLora = static_cast<SX1278*>(params);

    std::string message;

    uint32_t expectedSequence = 0;
    bool firstPacket = true;

    while (true) {
        if (pLora->receive(message)) {
            size_t separator = message.find('|');

            if (separator == std::string::npos) {
                printf("Invalid packet: %s\n", message.c_str());
                continue;
            }

            uint32_t sequence = static_cast<uint32_t>(std::stoul(message.substr(0, separator)));

            std::string payload = message.substr(separator + 1);

            if (firstPacket) {
                expectedSequence = sequence + 1;
                firstPacket = false;

            }
            else if (sequence == expectedSequence) {
                // Exactly what we expected.
                expectedSequence++;

            }
            else if (sequence > expectedSequence) {
                uint32_t lost = sequence - expectedSequence;
                printf("Lost %lu packet(s)\n", static_cast<unsigned long>(lost));
                expectedSequence = sequence + 1;

            }
            else {
                printf("Old/duplicate packet: %lu\n", static_cast<unsigned long>(sequence));
            }

            printf("RX seq=%lu: %s\n", static_cast<unsigned long>(sequence), payload.c_str());
        }

        vTaskDelay(pdMS_TO_TICKS(10));
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
        lora.startReceive();
        xTaskCreate(lora_receive_task, "LoRaReceiveTask", 512, (void*)&lora, LORA_RECEIVE_TASK_PRIORITY, nullptr);
        vTaskStartScheduler();
    }
    else {
        printf("SX1278 not detected\n");
    }    

    return 0;
}
