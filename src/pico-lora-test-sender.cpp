#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/spi.h"

#include <string>
#include <cstring>

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#define LORA_SEND_TASK_PRIORITY (tskIDLE_PRIORITY + 2UL)

SemaphoreHandle_t uart_mutex;

namespace lora_config {
    inline constexpr uint TX = 4;
    inline constexpr uint RX = 5;
}

void lora_send_task(void *params) {
    LoRa *pLora = static_cast<LoRa *>(pvParameters);

    std::string commsString = "comms data from pico";
    while (true) {
        pLora->send_lora_packet((unsigned char*)message, strlen(message), 0);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

int main( void )
{
    stdio_init_all();

    LoRa lora;
    lora.init(433, 10, "CODEBRANE");

    xTaskCreate(lora_send_task, "LoRaSendTask", 512, (void*)&lora, LORA_SEND_TASK_PRIORITY, nullptr);

    vTaskStartScheduler();

    return 0;
}
