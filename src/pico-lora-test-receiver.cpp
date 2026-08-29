#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/spi.h"

#include "FreeRTOS.h"
#include "task.h"

#include "SX1278.hpp"

#define LORA_RECEIVE_TASK_PRIORITY (tskIDLE_PRIORITY + 2UL)

#define UART_BUFFER_SIZE 256
#define MESSAGE_BUFFER_SIZE 256

namespace lora_config {
    inline spi_inst_t* SPI = spi0;
    inline constexpr uint SCK   = 18;
    inline constexpr uint MOSI  = 19;
    inline constexpr uint MISO  = 16;
    inline constexpr uint CS    = 17;
    inline constexpr uint RESET = 20;
}

void lora_receive_task(void* params) {
    SX1278 *pLora = static_cast<SX1278 *>(params);

    std::string message;

    while (true) {
        if (pLora->receive(message)) {
            printf("LoRa RX: %s\n", message.c_str());
        }

        vTaskDelay(pdMS_TO_TICKS(10));
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
        lora.startReceive();
        xTaskCreate(lora_receive_task, "LoRaReceiveTask", 512, (void*)&lora, LORA_RECEIVE_TASK_PRIORITY, nullptr);
        vTaskStartScheduler();
    }
    else {
        printf("SX1278 not detected\n");
    }    

    return 0;
}
