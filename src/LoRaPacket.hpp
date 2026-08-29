#pragma once

#include <cstdint>
#include <limits>

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
    float temperature;              // °C
    float humidity;                 // %
    float pressure;                 // hPa

    float windSpeed;                // mph
    float windGust;                 // mph
    uint16_t windDirectionDegrees;  // 0-359°

    float rainfall;                 // mm

    float lux;                      // lux

    float batteryVoltage;           // V

    uint32_t timestamp;             // Unix time (UTC)
};

static_assert(sizeof(PacketHeader) == 6);
static_assert(sizeof(WeatherPayload) == 38);

inline const char* windDirectionName(uint16_t degrees) {

    static constexpr const char* directions[] = {
        "N",   "NNE", "NE",  "ENE",
        "E",   "ESE", "SE",  "SSE",
        "S",   "SSW", "SW",  "WSW",
        "W",   "WNW", "NW",  "NNW"
    };

    degrees %= 360;

    uint8_t index =
        static_cast<uint8_t>(
            ((degrees * 10) + 112) / 225
        ) % 16;

    return directions[index];
}

/*
 * Guards to ensure struct layout and floating-point representation will work on the wire.
 * The protocol is little endian and is intended for use among pico and esp32.
 * Both platforms normally use 32-bit IEEE-754 float.
 */
static_assert(sizeof(float) == 4);
static_assert(std::numeric_limits<float>::is_iec559);
static_assert(sizeof(PacketHeader) == 6);
static_assert(sizeof(WeatherPayload) == 38);
