#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "hardware/spi.h"

enum class LoRaBandwidth : uint8_t {
    BW_7_8_KHZ   = 0,
    BW_10_4_KHZ  = 1,
    BW_15_6_KHZ  = 2,
    BW_20_8_KHZ  = 3,
    BW_31_25_KHZ = 4,
    BW_41_7_KHZ  = 5,
    BW_62_5_KHZ  = 6,
    BW_125_KHZ   = 7,
    BW_250_KHZ   = 8,
    BW_500_KHZ   = 9
};

enum class LoRaCodingRate : uint8_t {
    CR_4_5 = 1,
    CR_4_6 = 2,
    CR_4_7 = 3,
    CR_4_8 = 4
};

struct SX1278Config {
    uint32_t frequencyHz = 433000000;

    LoRaBandwidth bandwidth = LoRaBandwidth::BW_125_KHZ;
    LoRaCodingRate codingRate = LoRaCodingRate::CR_4_5;

    uint8_t spreadingFactor = 7;

    bool crcEnabled = true;

    uint16_t preambleLength = 8;

    uint8_t syncWord = 0x12;

    uint8_t txPowerDbm = 17;
};

class SX1278 {
public:
    SX1278(
        spi_inst_t* spi,
        uint csPin,
        uint resetPin,
        uint sckPin,
        uint mosiPin,
        uint misoPin
    );

    bool init(const SX1278Config& config = SX1278Config{});

    bool send(
        const uint8_t* data,
        size_t length,
        uint32_t timeoutMs = 5000
    );

    bool send(
        const std::string& data,
        uint32_t timeoutMs = 5000
    );

    bool receive(std::string& data);
    bool receive(std::vector<uint8_t>& data);

    void startReceive();

    void standby();
    void sleep();

    void setFrequency(uint32_t frequencyHz);
    void setTxPower(uint8_t powerDbm);

    uint8_t getVersion();

private:
    spi_inst_t* _spi;

    uint _csPin;
    uint _resetPin;
    uint _sckPin;
    uint _mosiPin;
    uint _misoPin;

    void reset();

    void setMode(uint8_t mode);

    void configureModem(
        const SX1278Config& config
    );

    uint8_t readRegister(uint8_t address);

    void writeRegister(
        uint8_t address,
        uint8_t value
    );

    void readBurst(
        uint8_t address,
        uint8_t* buffer,
        size_t length
    );

    void writeBurst(
        uint8_t address,
        const uint8_t* buffer,
        size_t length
    );
};