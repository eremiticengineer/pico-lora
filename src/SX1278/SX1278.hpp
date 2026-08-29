#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

#include "hardware/spi.h"

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

    bool init(uint32_t frequencyHz = 433000000);

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