#include "SX1278.hpp"

#include <algorithm>

#include "hardware/gpio.h"
#include "pico/stdlib.h"
#include "pico/time.h"

namespace {
    constexpr uint8_t REG_FIFO = 0x00;
    constexpr uint8_t REG_OP_MODE = 0x01;
    constexpr uint8_t REG_FRF_MSB = 0x06;
    constexpr uint8_t REG_FRF_MID = 0x07;
    constexpr uint8_t REG_FRF_LSB = 0x08;
    constexpr uint8_t REG_PA_CONFIG = 0x09;
    constexpr uint8_t REG_LNA = 0x0C;
    constexpr uint8_t REG_FIFO_ADDR_PTR = 0x0D;
    constexpr uint8_t REG_FIFO_TX_BASE_ADDR = 0x0E;
    constexpr uint8_t REG_FIFO_RX_BASE_ADDR = 0x0F;
    constexpr uint8_t REG_FIFO_RX_CURRENT_ADDR = 0x10;
    constexpr uint8_t REG_IRQ_FLAGS = 0x12;
    constexpr uint8_t REG_RX_NB_BYTES = 0x13;
    constexpr uint8_t REG_MODEM_CONFIG_1 = 0x1D;
    constexpr uint8_t REG_MODEM_CONFIG_2 = 0x1E;
    constexpr uint8_t REG_PREAMBLE_MSB = 0x20;
    constexpr uint8_t REG_PREAMBLE_LSB = 0x21;
    constexpr uint8_t REG_PAYLOAD_LENGTH = 0x22;
    constexpr uint8_t REG_MODEM_CONFIG_3 = 0x26;
    constexpr uint8_t REG_SYNC_WORD = 0x39;
    constexpr uint8_t REG_VERSION = 0x42;
    constexpr uint8_t REG_PKT_SNR_VALUE = 0x19;
    constexpr uint8_t REG_PKT_RSSI_VALUE = 0x1A;
    constexpr uint8_t MODE_LONG_RANGE_MODE = 0x80;
    constexpr uint8_t MODE_SLEEP = 0x00;
    constexpr uint8_t MODE_STANDBY = 0x01;
    constexpr uint8_t MODE_TX = 0x03;
    constexpr uint8_t MODE_RX_CONTINUOUS = 0x05;
    constexpr uint8_t IRQ_RX_DONE_MASK = 0x40;
    constexpr uint8_t IRQ_PAYLOAD_CRC_ERROR_MASK = 0x20;
    constexpr uint8_t IRQ_TX_DONE_MASK = 0x08;
    constexpr uint32_t FXOSC = 32000000;
    constexpr size_t MAX_PACKET_SIZE = 255;
}

SX1278::SX1278(spi_inst_t* spi, uint csPin, uint resetPin, uint sckPin, uint mosiPin, uint misoPin)
    : _spi(spi),
      _csPin(csPin),
      _resetPin(resetPin),
      _sckPin(sckPin),
      _mosiPin(mosiPin),
      _misoPin(misoPin) {}

bool SX1278::init(const SX1278Config& config) {
    /*
     * SX1278 SPI maximum is much higher than this,
     * but 1 MHz is a nice conservative starting point.
     */

    spi_init(_spi, 1000 * 1000);
    gpio_set_function(_sckPin, GPIO_FUNC_SPI);
    gpio_set_function(_mosiPin, GPIO_FUNC_SPI);
    gpio_set_function(_misoPin, GPIO_FUNC_SPI);
    gpio_init(_csPin);
    gpio_set_dir(_csPin, GPIO_OUT);
    gpio_put(_csPin, 1);
    gpio_init(_resetPin);
    gpio_set_dir(_resetPin, GPIO_OUT);
    reset();

    /*
     * SX1276/77/78/79 normally reports 0x12.
     */

    if (getVersion() != 0x12) {
        return false;
    }

    /*
     * Enter sleep first because LoRa mode
     * should be selected while sleeping.
     */

    writeRegister(REG_OP_MODE, MODE_SLEEP);
    writeRegister(REG_OP_MODE, MODE_LONG_RANGE_MODE | MODE_SLEEP);
    sleep_ms(10);
    standby();

    /*
     * FIFO base addresses.
     */

    writeRegister(REG_FIFO_TX_BASE_ADDR, 0x00);
    writeRegister(REG_FIFO_RX_BASE_ADDR, 0x00);
    setFrequency(config.frequencyHz);
    setTxPower(config.txPowerDbm);

    /*
     * Enable LNA boost.
     *
     * Existing value:
     *
     * xxxx xx00
     *
     * OR 0x03:
     *
     * xxxx xx11
     */

    writeRegister(REG_LNA, readRegister(REG_LNA) | 0x03);
    configureModem(config);

    /*
     * Preamble length.
     *
     * Example:
     *
     * 8 decimal = 0x0008
     *
     * MSB = 0x00
     * LSB = 0x08
     */

    writeRegister(REG_PREAMBLE_MSB, static_cast<uint8_t>(config.preambleLength >> 8));
    writeRegister(REG_PREAMBLE_LSB, static_cast<uint8_t>(config.preambleLength));
    writeRegister(REG_SYNC_WORD, config.syncWord);

    /*
     * Clear all IRQ flags.
     *
     * Writing 1 clears each corresponding flag.
     *
     * 1111 1111 = 0xFF
     */

    writeRegister(REG_IRQ_FLAGS, 0xFF);
    standby();
    return true;
}

void SX1278::configureModem(const SX1278Config& config) {
    /*
     * REG_MODEM_CONFIG_1
     *
     * bits 7:4 = bandwidth
     * bits 3:1 = coding rate
     * bit  0   = implicit header mode
     *
     * We use explicit header mode,
     * so bit 0 remains 0.
     *
     * Default:
     *
     * bandwidth 125 kHz = 7
     *
     *      0111
     *
     * coding rate 4/5 = 1
     *
     *      001
     *
     * explicit header = 0
     *
     * Result:
     *
     *      0111 0010
     *
     *      = 0x72
     */

    uint8_t modemConfig1 = (static_cast<uint8_t>(config.bandwidth) << 4) | (static_cast<uint8_t>(config.codingRate) << 1);
    writeRegister(REG_MODEM_CONFIG_1, modemConfig1);

    /*
     * REG_MODEM_CONFIG_2
     *
     * bits 7:4 = spreading factor
     * bit  3   = TX continuous mode
     * bit  2   = RX payload CRC
     * bits 1:0 = symbol timeout MSBs
     *
     *
     * Default SF7:
     *
     *      0111 xxxx
     *
     * CRC enabled:
     *
     *      xxxx x1xx
     *
     * Result:
     *
     *      0111 0100
     *
     *      = 0x74
     */

    uint8_t spreadingFactor = std::clamp<uint8_t>(config.spreadingFactor, 6, 12);
    uint8_t modemConfig2 = spreadingFactor << 4;
    if (config.crcEnabled) {
        modemConfig2 |= (1 << 2);

        /*
         * Bit 2:
         *
         * 0000 0100
         *
         * = 0x04
         */
    }
    writeRegister(REG_MODEM_CONFIG_2, modemConfig2);

    /*
     * REG_MODEM_CONFIG_3
     *
     * bit 3 = low data rate optimisation
     * bit 2 = AGC auto
     *
     * For now:
     *
     *      0000 0100
     *
     * AGC enabled.
     */

    uint8_t modemConfig3 = 0x04;

    /*
     * Low Data Rate Optimisation should be
     * enabled when symbol duration exceeds 16 ms.
     *
     * Tsymbol = 2^SF / bandwidth
     *
     * Commonly required for:
     *
     * SF11 / 125 kHz
     * SF12 / 125 kHz
     *
     * We'll automatically enable it for
     * those common configurations.
     */

    if (config.bandwidth == LoRaBandwidth::BW_125_KHZ && spreadingFactor >= 11) {
        /*
         * Enable bit 3:
         *
         * 0000 1000
         *
         * plus AGC:
         *
         * 0000 0100
         *
         * gives:
         *
         * 0000 1100
         *
         * = 0x0C
         */

        modemConfig3 |= (1 << 3);
    }
    writeRegister(REG_MODEM_CONFIG_3, modemConfig3);
}

void SX1278::reset() {
    gpio_put(_resetPin, 0);
    sleep_ms(1);
    gpio_put(_resetPin, 1);
    sleep_ms(10);
}

uint8_t SX1278::getVersion() {
    return readRegister(REG_VERSION);
}

void SX1278::setFrequency(uint32_t frequencyHz) {
    /*
     * SX1278 frequency register:
     *
     * FRF =
     *
     * frequency * 2^19 / FXOSC
     *
     * where FXOSC = 32 MHz
     */

    uint64_t frf = (static_cast<uint64_t>(frequencyHz) << 19) / FXOSC;
    writeRegister(REG_FRF_MSB, static_cast<uint8_t>(frf >> 16));
    writeRegister(REG_FRF_MID, static_cast<uint8_t>(frf >> 8));
    writeRegister(REG_FRF_LSB, static_cast<uint8_t>(frf));
}

void SX1278::setTxPower(uint8_t powerDbm) {
    /*
     * PA_BOOST output.
     *
     * Clamp to the normal 2-17 dBm range.
     */

    powerDbm = std::clamp<uint8_t>(powerDbm, 2, 17);

    /*
     * REG_PA_CONFIG
     *
     * bit 7 = PA_BOOST
     *
     *      1000 0000
     *
     *      = 0x80
     *
     * bits 3:0 = OutputPower
     *
     * Pout = 2 + OutputPower
     *
     * Example 17 dBm:
     *
     * OutputPower = 15
     *
     *      0000 1111
     *
     * PA_BOOST:
     *
     *      1000 0000
     *
     * Result:
     *
     *      1000 1111
     *
     *      = 0x8F
     */

    writeRegister(REG_PA_CONFIG, 0x80 | (powerDbm - 2));
}

bool SX1278::send(const std::string& data, uint32_t timeoutMs) {
    return send(reinterpret_cast<const uint8_t*>(data.data()), data.size(), timeoutMs);
}

bool SX1278::send(const uint8_t* data, size_t length, uint32_t timeoutMs) {
    if (data == nullptr || length == 0 || length > MAX_PACKET_SIZE) {
        return false;
    }
    standby();
    writeRegister(REG_IRQ_FLAGS, 0xFF);
    writeRegister(REG_FIFO_ADDR_PTR, 0x00);
    writeBurst(REG_FIFO, data, length);
    writeRegister(REG_PAYLOAD_LENGTH, static_cast<uint8_t>(length));
    setMode(MODE_TX);
    const absolute_time_t timeout = make_timeout_time_ms(timeoutMs);
    while (true) {
        const uint8_t flags = readRegister(REG_IRQ_FLAGS);
        if (flags & IRQ_TX_DONE_MASK) {
            writeRegister(REG_IRQ_FLAGS, IRQ_TX_DONE_MASK);
            standby();
            return true;
        }
        if (absolute_time_diff_us(get_absolute_time(), timeout) <= 0) {
            standby();
            return false;
        }
        sleep_us(100);
    }
}

void SX1278::startReceive() {
    standby();
    writeRegister(REG_IRQ_FLAGS, 0xFF);
    writeRegister(REG_FIFO_ADDR_PTR, 0x00);
    setMode(MODE_RX_CONTINUOUS);
}

bool SX1278::receive(std::string& data) {
    const uint8_t flags = readRegister(REG_IRQ_FLAGS);
    if (!(flags & IRQ_RX_DONE_MASK)) {
        return false;
    }
    if (flags & IRQ_PAYLOAD_CRC_ERROR_MASK) {
        writeRegister(REG_IRQ_FLAGS, 0xFF);
        return false;
    }
    const uint8_t length = readRegister(REG_RX_NB_BYTES);
    const uint8_t fifoAddress = readRegister(REG_FIFO_RX_CURRENT_ADDR);
    writeRegister(REG_FIFO_ADDR_PTR, fifoAddress);
    data.resize(length);
    if (length > 0) {
        readBurst(REG_FIFO, reinterpret_cast<uint8_t*>(data.data()), length);
    }
    writeRegister(REG_IRQ_FLAGS, 0xFF);
    return true;
}

bool SX1278::receive(std::vector<uint8_t>& data) {
    const uint8_t flags = readRegister(REG_IRQ_FLAGS);
    if (!(flags & IRQ_RX_DONE_MASK)) {
        return false;
    }
    if (flags & IRQ_PAYLOAD_CRC_ERROR_MASK) {
        writeRegister(REG_IRQ_FLAGS, 0xFF);
        return false;
    }
    const uint8_t length = readRegister(REG_RX_NB_BYTES);
    const uint8_t fifoAddress = readRegister(REG_FIFO_RX_CURRENT_ADDR);
    writeRegister(REG_FIFO_ADDR_PTR, fifoAddress);
    data.resize(length);
    if (length > 0) {
        readBurst(REG_FIFO, data.data(), length);
    }
    writeRegister(REG_IRQ_FLAGS, 0xFF);
    return true;
}

int16_t SX1278::getPacketRssi() {
    const uint8_t value = readRegister(REG_PKT_RSSI_VALUE);

    /*
     * SX1278 at 433 MHz uses the LF RSSI offset.
     *
     * RSSI[dBm] =
     *
     * -164 + RegPktRssiValue
     */

    return -164 + static_cast<int16_t>(value);
}

float SX1278::getPacketSnr() {
    const int8_t value = static_cast<int8_t>(readRegister(REG_PKT_SNR_VALUE));

    /*
     * Each register step = 0.25 dB.
     *
     * Examples:
     *
     *  20 ->  5.0 dB
     *   4 ->  1.0 dB
     *  -4 -> -1.0 dB
     * -20 -> -5.0 dB
     */

    return static_cast<float>(value) / 4.0f;
}

void SX1278::standby() {
    setMode(MODE_STANDBY);
}

void SX1278::sleep() {
    setMode(MODE_SLEEP);
}

void SX1278::setMode(uint8_t mode) {
    writeRegister(REG_OP_MODE, MODE_LONG_RANGE_MODE | mode);
}

uint8_t SX1278::readRegister(uint8_t address) {
    uint8_t tx[] = {static_cast<uint8_t>(address & 0x7F), 0x00};
    uint8_t rx[2] = {};
    gpio_put(_csPin, 0);
    spi_write_read_blocking(_spi, tx, rx, 2);
    gpio_put(_csPin, 1);
    return rx[1];
}

void SX1278::writeRegister(uint8_t address, uint8_t value) {
    uint8_t buffer[] = {
        static_cast<uint8_t>(address | 0x80),
        value
    };
    gpio_put(_csPin, 0);
    spi_write_blocking(_spi, buffer, sizeof(buffer));
    gpio_put(_csPin, 1);
}

void SX1278::writeBurst(uint8_t address, const uint8_t* buffer, size_t length) {
    const uint8_t registerAddress = address | 0x80;
    gpio_put(_csPin, 0);
    spi_write_blocking(_spi, &registerAddress, 1);
    spi_write_blocking(_spi, buffer, length);
    gpio_put(_csPin, 1);
}

void SX1278::readBurst(uint8_t address, uint8_t* buffer, size_t length) {
    const uint8_t registerAddress = address & 0x7F;
    gpio_put(_csPin, 0);
    spi_write_blocking(_spi, &registerAddress, 1);
    spi_read_blocking(_spi, 0x00, buffer, length);
    gpio_put(_csPin, 1);
}
