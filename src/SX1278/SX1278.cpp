#include "SX1278.hpp"

#include <algorithm>

#include "hardware/gpio.h"
#include "pico/stdlib.h"
#include "pico/time.h"

namespace {

// Registers

constexpr uint8_t REG_FIFO                 = 0x00;
constexpr uint8_t REG_OP_MODE              = 0x01;

constexpr uint8_t REG_FRF_MSB              = 0x06;
constexpr uint8_t REG_FRF_MID              = 0x07;
constexpr uint8_t REG_FRF_LSB              = 0x08;

constexpr uint8_t REG_PA_CONFIG            = 0x09;
constexpr uint8_t REG_LNA                  = 0x0C;

constexpr uint8_t REG_FIFO_ADDR_PTR        = 0x0D;
constexpr uint8_t REG_FIFO_TX_BASE_ADDR    = 0x0E;
constexpr uint8_t REG_FIFO_RX_BASE_ADDR    = 0x0F;
constexpr uint8_t REG_FIFO_RX_CURRENT_ADDR = 0x10;

constexpr uint8_t REG_IRQ_FLAGS            = 0x12;
constexpr uint8_t REG_RX_NB_BYTES          = 0x13;

constexpr uint8_t REG_MODEM_CONFIG_1       = 0x1D;
constexpr uint8_t REG_MODEM_CONFIG_2       = 0x1E;

constexpr uint8_t REG_PREAMBLE_MSB         = 0x20;
constexpr uint8_t REG_PREAMBLE_LSB         = 0x21;

constexpr uint8_t REG_PAYLOAD_LENGTH       = 0x22;

constexpr uint8_t REG_MODEM_CONFIG_3       = 0x26;

constexpr uint8_t REG_SYNC_WORD            = 0x39;

constexpr uint8_t REG_VERSION              = 0x42;


// Operating modes

constexpr uint8_t MODE_LONG_RANGE_MODE = 0x80;

constexpr uint8_t MODE_SLEEP         = 0x00;
constexpr uint8_t MODE_STANDBY       = 0x01;
constexpr uint8_t MODE_TX            = 0x03;
constexpr uint8_t MODE_RX_CONTINUOUS = 0x05;


// IRQ flags

constexpr uint8_t IRQ_RX_DONE_MASK          = 0x40;
constexpr uint8_t IRQ_PAYLOAD_CRC_ERROR_MASK = 0x20;
constexpr uint8_t IRQ_TX_DONE_MASK          = 0x08;


// SX1278 oscillator frequency

constexpr uint32_t FXOSC = 32000000;


// Maximum LoRa packet size

constexpr size_t MAX_PACKET_SIZE = 255;

}


SX1278::SX1278(
    spi_inst_t* spi,
    uint csPin,
    uint resetPin,
    uint sckPin,
    uint mosiPin,
    uint misoPin
)
    : _spi(spi),
      _csPin(csPin),
      _resetPin(resetPin),
      _sckPin(sckPin),
      _mosiPin(mosiPin),
      _misoPin(misoPin) {
}


bool SX1278::init(uint32_t frequencyHz) {

    // SX1278 supports SPI up to 10 MHz.
    // 1 MHz is deliberately conservative for initial testing.

    spi_init(_spi, 1000 * 1000);

    gpio_set_function(_sckPin, GPIO_FUNC_SPI);
    gpio_set_function(_mosiPin, GPIO_FUNC_SPI);
    gpio_set_function(_misoPin, GPIO_FUNC_SPI);


    // Chip select

    gpio_init(_csPin);
    gpio_set_dir(_csPin, GPIO_OUT);
    gpio_put(_csPin, 1);


    // Reset

    gpio_init(_resetPin);
    gpio_set_dir(_resetPin, GPIO_OUT);

    reset();


    // SX1276/77/78/79 reports version 0x12.

    if (getVersion() != 0x12) {
        return false;
    }


    // Enter sleep before enabling LoRa mode.

    writeRegister(
        REG_OP_MODE,
        MODE_SLEEP
    );

    writeRegister(
        REG_OP_MODE,
        MODE_LONG_RANGE_MODE | MODE_SLEEP
    );


    sleep_ms(10);

    standby();


    // FIFO layout

    writeRegister(
        REG_FIFO_TX_BASE_ADDR,
        0x00
    );

    writeRegister(
        REG_FIFO_RX_BASE_ADDR,
        0x00
    );


    setFrequency(frequencyHz);


    /*
     * PA_BOOST output.
     *
     * Default power = 17 dBm.
     */

    setTxPower(17);


    /*
     * Improve sensitivity.
     *
     * Bits 1:0 = 11 enables LNA boost.
     */

    writeRegister(
        REG_LNA,
        readRegister(REG_LNA) | 0x03
    );


    /*
     * ModemConfig1
     *
     * bits 7:4 bandwidth:
     *      0111 = 125 kHz
     *
     * bits 3:1 coding rate:
     *      001 = 4/5
     *
     * bit 0:
     *      0 = explicit header
     *
     * 0111 0010 = 0x72
     */

    writeRegister(
        REG_MODEM_CONFIG_1,
        0x72
    );


    /*
     * ModemConfig2
     *
     * bits 7:4:
     *      0111 = spreading factor 7
     *
     * bit 2:
     *      1 = payload CRC enabled
     *
     * 0111 0100 = 0x74
     */

    writeRegister(
        REG_MODEM_CONFIG_2,
        0x74
    );


    /*
     * ModemConfig3
     *
     * bit 2:
     *      AGC auto enabled
     */

    writeRegister(
        REG_MODEM_CONFIG_3,
        0x04
    );


    // Preamble length = 8 symbols

    writeRegister(
        REG_PREAMBLE_MSB,
        0x00
    );

    writeRegister(
        REG_PREAMBLE_LSB,
        0x08
    );


    /*
     * LoRa public/default sync word.
     *
     * Both radios MUST use the same value.
     */

    writeRegister(
        REG_SYNC_WORD,
        0x12
    );


    // Clear any pending IRQs.

    writeRegister(
        REG_IRQ_FLAGS,
        0xFF
    );

    standby();

    return true;
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
     * FRF =
     *
     * frequency * 2^19 / FXOSC
     */

    uint64_t frf =
        (static_cast<uint64_t>(frequencyHz) << 19)
        / FXOSC;

    writeRegister(
        REG_FRF_MSB,
        static_cast<uint8_t>(frf >> 16)
    );

    writeRegister(
        REG_FRF_MID,
        static_cast<uint8_t>(frf >> 8)
    );

    writeRegister(
        REG_FRF_LSB,
        static_cast<uint8_t>(frf)
    );
}


void SX1278::setTxPower(uint8_t powerDbm) {

    /*
     * Using PA_BOOST.
     *
     * Normal useful range:
     *
     * 2-17 dBm
     */

    powerDbm = std::clamp<uint8_t>(
        powerDbm,
        2,
        17
    );


    /*
     * PA_BOOST = bit 7
     *
     * Output power:
     *
     * Pout = 2 + OutputPower
     */

    writeRegister(
        REG_PA_CONFIG,
        0x80 | (powerDbm - 2)
    );
}


bool SX1278::send(
    const std::string& data,
    uint32_t timeoutMs
) {

    return send(
        reinterpret_cast<const uint8_t*>(data.data()),
        data.size(),
        timeoutMs
    );
}


bool SX1278::send(
    const uint8_t* data,
    size_t length,
    uint32_t timeoutMs
) {

    if (data == nullptr || length == 0) {
        return false;
    }

    if (length > MAX_PACKET_SIZE) {
        return false;
    }


    standby();


    // Clear previous IRQs.

    writeRegister(
        REG_IRQ_FLAGS,
        0xFF
    );


    // Start writing at TX FIFO base.

    writeRegister(
        REG_FIFO_ADDR_PTR,
        0x00
    );


    writeBurst(
        REG_FIFO,
        data,
        length
    );


    writeRegister(
        REG_PAYLOAD_LENGTH,
        static_cast<uint8_t>(length)
    );


    // Start transmitting.

    setMode(MODE_TX);


    const absolute_time_t timeout =
        make_timeout_time_ms(timeoutMs);


    while (true) {

        const uint8_t flags =
            readRegister(REG_IRQ_FLAGS);


        if (flags & IRQ_TX_DONE_MASK) {

            writeRegister(
                REG_IRQ_FLAGS,
                IRQ_TX_DONE_MASK
            );

            standby();

            return true;
        }


        if (absolute_time_diff_us(
                get_absolute_time(),
                timeout
            ) <= 0) {

            standby();

            return false;
        }


        /*
         * Tiny wait so we aren't hammering SPI continuously.
         */

        sleep_us(100);
    }
}


void SX1278::startReceive() {

    standby();


    writeRegister(
        REG_IRQ_FLAGS,
        0xFF
    );


    writeRegister(
        REG_FIFO_ADDR_PTR,
        0x00
    );


    setMode(
        MODE_RX_CONTINUOUS
    );
}


bool SX1278::receive(std::string& data) {

    const uint8_t flags =
        readRegister(REG_IRQ_FLAGS);


    if (!(flags & IRQ_RX_DONE_MASK)) {
        return false;
    }


    // CRC failure.

    if (flags & IRQ_PAYLOAD_CRC_ERROR_MASK) {

        writeRegister(
            REG_IRQ_FLAGS,
            0xFF
        );

        return false;
    }


    const uint8_t length =
        readRegister(REG_RX_NB_BYTES);


    const uint8_t fifoAddress =
        readRegister(REG_FIFO_RX_CURRENT_ADDR);


    writeRegister(
        REG_FIFO_ADDR_PTR,
        fifoAddress
    );


    data.resize(length);


    if (length > 0) {

        readBurst(
            REG_FIFO,
            reinterpret_cast<uint8_t*>(data.data()),
            length
        );
    }


    // Clear IRQ flags.

    writeRegister(
        REG_IRQ_FLAGS,
        0xFF
    );


    return true;
}


void SX1278::standby() {

    setMode(MODE_STANDBY);
}


void SX1278::sleep() {

    setMode(MODE_SLEEP);
}


void SX1278::setMode(uint8_t mode) {

    writeRegister(
        REG_OP_MODE,
        MODE_LONG_RANGE_MODE | mode
    );
}


uint8_t SX1278::readRegister(uint8_t address) {

    uint8_t tx[] = {
        static_cast<uint8_t>(address & 0x7F),
        0x00
    };

    uint8_t rx[2] = {};


    gpio_put(_csPin, 0);

    spi_write_read_blocking(
        _spi,
        tx,
        rx,
        2
    );

    gpio_put(_csPin, 1);


    return rx[1];
}


void SX1278::writeRegister(
    uint8_t address,
    uint8_t value
) {

    uint8_t buffer[] = {

        static_cast<uint8_t>(
            address | 0x80
        ),

        value
    };


    gpio_put(_csPin, 0);

    spi_write_blocking(
        _spi,
        buffer,
        sizeof(buffer)
    );

    gpio_put(_csPin, 1);
}


void SX1278::writeBurst(
    uint8_t address,
    const uint8_t* buffer,
    size_t length
) {

    const uint8_t registerAddress =
        address | 0x80;


    gpio_put(_csPin, 0);


    spi_write_blocking(
        _spi,
        &registerAddress,
        1
    );


    spi_write_blocking(
        _spi,
        buffer,
        length
    );


    gpio_put(_csPin, 1);
}


void SX1278::readBurst(
    uint8_t address,
    uint8_t* buffer,
    size_t length
) {

    const uint8_t registerAddress =
        address & 0x7F;


    gpio_put(_csPin, 0);


    spi_write_blocking(
        _spi,
        &registerAddress,
        1
    );


    spi_read_blocking(
        _spi,
        0x00,
        buffer,
        length
    );


    gpio_put(_csPin, 1);
}