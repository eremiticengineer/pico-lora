#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/spi.h"

#include "FreeRTOS.h"
#include "task.h"

#include <cstring>
#include <vector>

#include "LoRaPacket.hpp"
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

    std::vector<uint8_t> packet;

    uint32_t expectedSequence = 0;
    bool firstPacket = true;

    while (true) {
        if (pLora->receive(packet)) {
            const int16_t rssi = pLora->getPacketRssi();
            const float snr = pLora->getPacketSnr();

            if (packet.size() < sizeof(PacketHeader)) {
                printf("Invalid packet: too short (%u bytes)\n",
                    static_cast<unsigned>(packet.size()));
                vTaskDelay(pdMS_TO_TICKS(10));
                continue;
            }

            PacketHeader header;

            std::memcpy(&header, packet.data(), sizeof(PacketHeader));

            const uint32_t sequence = header.sequence;

            if (firstPacket) {
                expectedSequence = sequence + 1;
                firstPacket = false;
            }
            else if (sequence == expectedSequence) {
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

            const size_t payloadLength = packet.size() - sizeof(PacketHeader);

            std::string payload(reinterpret_cast<const char*>
                (
                    packet.data() + sizeof(PacketHeader)
                ),
                payloadLength
            );

            printf("RX seq=%lu version=%u type=%u: rssi:%ddBm, snr:%.1fdB %s\n",
                static_cast<unsigned long>(header.sequence),
                static_cast<unsigned>(header.version),
                static_cast<unsigned>(header.type),
                rssi, snr,
                payload.c_str()
            );
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void lora_receive_weather_data_task(void* params) {
    SX1278* pLora = static_cast<SX1278*>(params);

    std::vector<uint8_t> packet;

    uint32_t expectedSequence = 0;
    bool firstPacket = true;

    LoRaStats stats;

    while (true) {
        if (pLora->receive(packet)) {
            /*
             * Read these immediately after receiving
             * the packet. They refer to the most
             * recently received LoRa packet.
             */            
            const int16_t rssi = pLora->getPacketRssi();
            const float snr = pLora->getPacketSnr();

            if (packet.size() < sizeof(PacketHeader)) {
                printf("Invalid packet: too short\n");
                vTaskDelay(pdMS_TO_TICKS(10));
                continue;
            }

            PacketHeader header;

            std::memcpy(&header, packet.data(), sizeof(PacketHeader));

            const uint32_t sequence = header.sequence;

            /*
             * Sequence tracking.
             */
            if (firstPacket) {
                expectedSequence = sequence + 1;
                firstPacket = false;
            }
            else if (sequence == expectedSequence) {
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

            /*
             * Add this successfully received packet
             * to the running radio statistics.
             */
            stats.addPacket(rssi, snr);

            switch (header.type) {
                case PacketType::Weather: {
                    const size_t expectedSize = sizeof(PacketHeader) + sizeof(WeatherPayload);

                    if (packet.size() != expectedSize) {
                        printf("Invalid weather packet size: %u\n", static_cast<unsigned>
                            (
                                packet.size()
                            )
                        );
                        break;
                    }

                    WeatherPayload weather;

                    std::memcpy(&weather, packet.data() + sizeof(PacketHeader),
                        sizeof(WeatherPayload));

                    printf(
                        "RX seq=%lu "
                        "RSSI=%ddBm "
                        "SNR=%.1fdB "
                        "temp=%.1fC "
                        "humidity=%.1f%% "
                        "pressure=%.1fhPa "
                        "wind=%.1fmph "
                        "gust=%.1fmph "
                        "direction=%u "
                        "rain=%.1fmm "
                        "lux=%.1f "
                        "battery=%.2fV "
                        "timestamp=%lu\n",

                        static_cast<unsigned long>(header.sequence),

                        rssi,
                        snr,

                        weather.temperature,
                        weather.humidity,
                        weather.pressure,

                        weather.windSpeed,
                        weather.windGust,
                        weather.windDirectionDegrees,

                        weather.rainfall,

                        weather.lux,

                        weather.batteryVoltage,

                        static_cast<unsigned long>(weather.timestamp)
                    );

                    printf("direction=%u° (%s)\n", weather.windDirectionDegrees,
                        windDirectionName(weather.windDirectionDegrees));
                    
                    printf(
                        "LINK "
                        "received=%llu "
                        "lost=%llu "
                        "loss=%.2f%% "
                        "RSSI avg=%.1fdBm "
                        "min=%ddBm "
                        "SNR avg=%.1fdB "
                        "min=%.1fdB\n",

                        static_cast<unsigned long long>(stats.packetsReceived),

                        static_cast<unsigned long long>(stats.packetsLost),

                        stats.packetLossPercentage(),

                        stats.averageRssi(),
                        stats.minimumRssi,

                        stats.averageSnr(),
                        stats.minimumSnr
                    );

                    break;
                }

                case PacketType::Status:
                    printf("Received status packet\n");
                    break;

                case PacketType::Debug:
                    printf("Received debug packet\n");
                    break;

                default:
                    printf("Unknown packet type: %u\n", static_cast<unsigned>(header.type));
                    break;
            }
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
        //xTaskCreate(lora_receive_task, "LoRaReceiveTask", 512, (void*)&lora, LORA_RECEIVE_TASK_PRIORITY, nullptr);
        xTaskCreate(lora_receive_weather_data_task, "LoRaReceiveWeatherDataTask", 512, (void*)&lora, LORA_RECEIVE_TASK_PRIORITY, nullptr);
        vTaskStartScheduler();
    }
    else {
        printf("SX1278 not detected\n");
    }    

    return 0;
}
