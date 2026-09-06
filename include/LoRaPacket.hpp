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
    uint32_t sessionId;
    uint8_t version;
    PacketType type;
};

constexpr uint8_t SENSOR_VALID_BME280         = 1u << 0;
constexpr uint8_t SENSOR_VALID_VEML7700       = 1u << 1;
constexpr uint8_t SENSOR_VALID_WIND_DIRECTION = 1u << 2;
constexpr uint8_t SENSOR_VALID_WIND_SPEED     = 1u << 3;
constexpr uint8_t SENSOR_VALID_DS3231         = 1u << 4;

struct __attribute__((packed)) WeatherPayload {
    uint32_t timestamp;             // Unix time (UTC)

    uint32_t bootId;                // To detect a weather station reboot affecting rainTipsSinceBoot

    float temperature;              // °C
    float humidity;                 // %
    float pressure;                 // hPa

    float windSpeed;                // mph
    float windGust;                 // mph
    char windDirectionName[4];      // SW
    uint16_t windDirectionDegrees;  // 0-359

    float lux;                      // lux

    uint32_t rainTipsSinceBoot;     // mm

    float batteryVoltage;           // V

    uint8_t validSensors;
};

static_assert(sizeof(PacketHeader) == 10);
static_assert(sizeof(WeatherPayload) == 47);

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

struct LoRaStats {
    uint64_t packetsReceived = 0;
    uint64_t packetsLost = 0;

    int16_t minimumRssi = 0;
    int64_t totalRssi = 0;

    float minimumSnr = 0.0f;
    double totalSnr = 0.0;

    bool initialized = false;

    void addPacket(int16_t rssi, float snr) {
        if (!initialized) {
            minimumRssi = rssi;
            minimumSnr = snr;
            initialized = true;
        }
        else {
            if (rssi < minimumRssi) {
                minimumRssi = rssi;
            }

            if (snr < minimumSnr) {
                minimumSnr = snr;
            }
        }

        totalRssi += rssi;
        totalSnr += snr;

        packetsReceived++;
    }

    void addLostPackets(uint32_t count) {
        packetsLost += count;
    }

    float averageRssi() const {
        if (packetsReceived == 0) {
            return 0.0f;
        }

        return static_cast<float>(totalRssi) / static_cast<float>(packetsReceived);
    }

    float averageSnr() const {
        if (packetsReceived == 0) {
            return 0.0f;
        }

        return static_cast<float>(totalSnr / static_cast<double>(packetsReceived));
    }

    float packetLossPercentage() const {
        const uint64_t totalPackets = packetsReceived + packetsLost;

        if (totalPackets == 0) {
            return 0.0f;
        }

        return (static_cast<float>(packetsLost) / static_cast<float>(totalPackets)) * 100.0f;
    }
};

/*
 * Guards to ensure struct layout and floating-point representation will work on the wire.
 * The protocol is little endian and is intended for use among pico and esp32.
 * Both platforms normally use 32-bit IEEE-754 float.
 */
static_assert(sizeof(float) == 4);
static_assert(std::numeric_limits<float>::is_iec559);
static_assert(sizeof(PacketHeader) == 10);
static_assert(sizeof(WeatherPayload) == 47);
