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
    SX1278 *pLora = static_cast<SX1278 *>(params);

    uint32_t counter = 0;

    while (true) {
        std::string message = "Hello from Pico: " + std::to_string(counter++);

        if (pLora->send(message)) {
            printf("LoRa TX: %s\n", message.c_str());
        }
        else {
            printf("LoRa TX failed\n");
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

int main( void )
{
    stdio_init_all();

    SX1278 lora(
        lora_config::SPI,
        lora_config::CS,
        lora_config::RESET,
        lora_config::SCK,
        lora_config::MOSI,
        lora_config::MISO
    );

    if (lora.init(433000000)) {
        printf("SX1278 detected, version: 0x%02X\n", lora.getVersion());
        xTaskCreate(lora_send_task, "LoRaSendTask", 512, (void*)&lora, LORA_SEND_TASK_PRIORITY, nullptr);
        vTaskStartScheduler();
    }
    else {
        printf("SX1278 not detected\n");
    }    

    return 0;
}
