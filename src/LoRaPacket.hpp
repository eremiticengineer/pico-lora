#pragma once

#include <cstdint>

enum class PacketType : uint8_t {
    Weather = 1,
    Status  = 2,
    Debug   = 3
};

struct __attribute__((packed)) PacketHeader {
    uint32_t sequence;
    uint8_t version;
    PacketType type;
};

struct __attribute__((packed)) WeatherPayload {
    float temperature;
    float humidity;
    float pressure;
};