#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/uart.h"

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#define UART_RECEIVE_TASK_PRIORITY (tskIDLE_PRIORITY + 2UL)

#define UART_BUFFER_SIZE 256
#define MESSAGE_BUFFER_SIZE 256

#define MESSAGE_END_MARKER '#'

SemaphoreHandle_t uart_mutex;

volatile uint8_t ring_buffer[UART_BUFFER_SIZE];
unsigned char uart_buffer[UART_BUFFER_SIZE];
volatile uint16_t write_index = 0;
volatile uint16_t read_index = 0;
uint16_t message_index = 0;
uint8_t message_buffer[MESSAGE_BUFFER_SIZE];

namespace uart_config {
    inline uart_inst_t* const UART_NUM = uart0;
    inline constexpr uint BAUD = 115200;
    inline constexpr uint TX = 0;
    inline constexpr uint RX = 1;
}

// Call this from UART RX interrupt or polling when a byte arrives
void uart_receive_byte(uint8_t byte) {
  ring_buffer[write_index] = byte;
  write_index = (write_index + 1) % UART_BUFFER_SIZE;
}

void on_uart_rx() {
  while (uart_is_readable(uart_config::UART_NUM)) {
      char c = uart_getc(uart_config::UART_NUM);
      uart_receive_byte(c);
  }
}

// Check if there is data available
uint16_t uart_available() {
  if (write_index >= read_index)
    return write_index - read_index;
  else
    return UART_BUFFER_SIZE - (read_index - write_index);
}

// Read one byte from buffer if available
int uart_read_byte(uint8_t* byte) {
    if (uart_available() == 0) return 0; // no data

    *byte = ring_buffer[read_index];
    read_index = (read_index + 1) % UART_BUFFER_SIZE;
    return 1; // success
}

void init_uart() {
  uart_init(uart_config::UART_NUM, uart_config::BAUD);
  gpio_set_function(uart_config::TX, GPIO_FUNC_UART);
  gpio_set_function(uart_config::RX, GPIO_FUNC_UART);

  // Enable UART RX interrupt
  uart_set_irq_enables(uart_config::UART_NUM, true, false);  // RX only
  irq_set_exclusive_handler(UART0_IRQ, on_uart_rx);
  irq_set_enabled(UART0_IRQ, true);
}

void process_uart() {
    uint8_t byte;

    while (uart_read_byte(&byte)) {
        if (byte == MESSAGE_END_MARKER) {
            message_buffer[message_index] = '\0';
            printf("received: %s\n", message_buffer);
            message_index = 0;
        }
        else {
            if (message_index < MESSAGE_BUFFER_SIZE - 1) {
                message_buffer[message_index++] = static_cast<char>(byte);
            }
            else {
                // Buffer overflow - discard current message
                message_index = 0;
            }
        }
    }
}

void uart_receive_task(__unused void *params) {
    while (true) {
        process_uart();
        vTaskDelay(pdMS_TO_TICKS(25)); // 25ms
    }
}

int main( void )
{
    stdio_init_all();

    init_uart();

    uart_mutex = xSemaphoreCreateMutex();

    xTaskCreate(uart_receive_task, "UartReceiveTask", 512, nullptr, UART_RECEIVE_TASK_PRIORITY, nullptr);

    vTaskStartScheduler();

    return 0;
}
