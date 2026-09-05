# Pico LoRa Test

This is a simple project to send data from one pico to another over LoRa. Each pico is connected to an SX1278 433MHz module. The project has a simple text message format but also has a more complex protocol with header, payload type and payload as this is a test harness for my remote, solar powered weather station. It also collects average, minimum and maximum RSSI and SNR as well as detecting lost packets so I can test the range.

## Cloning the project

Clone the project with FreeRTOS submodules to get the pico functionality:

```
git clone --recurse-submodules https://github.com/eremiticengineer/pico-lora
```

If you cloned without recursing submodules:

```
git submodule update --init --recursive
```

## Building for different chips

pico2:

```
./build_project

or

./build_project pico2
```

Seeed Xaio rp2040:

```
./build_project seeed_xiao_rp2040
```

## Wiring the pico to the SX1278

The SPI configurations are in:

```
src/pico-lora-test-sender.cpp lora_config
src/pico-lora-test-receiver.cpp lora_config
```

adjust to suit. Wire thus:

| Pico | SX1278 |
|------|--------|
| 3V3  | 3.3V   |
| GND  | GND    |
| GP19 | MOSI   |
| GP16 | MISO   |
| GP18 | SCK    |
| GP17 | NSS    |

## Radio settings

Both the sender and receiver are using these settings:

| Setting          | Value          |
|------------------|----------------|
| Frequency        | 433.92 MHz     |
| Bandwidth        | 125 kHz        |
| Spreading Factor | SF7            |
| Coding Rate      | 4/5            |
| Header           | HeaderExplicit |
| CRC              | Enabled        |
| Preamble         | 8 symbols      |
| Sync word        | 0x12           |
| TX power         | 10 dBm         |

## RSSI

A rough guide to LoRa RSSI is:

|             RSSI | Rough interpretation      |
| ---------------: | ------------------------- |
|   -40 to -70 dBm | Very strong               |
|   -70 to -90 dBm | Strong/good               |
|  -90 to -110 dBm | Usable                    |
| -110 to -120 dBm | Weak                      |
|   below -120 dBm | Getting close to the edge |

Negative SNR can still give completely valid reception. With SF7/125 kHz, though, you won't get as far below the noise floor as you could with higher spreading factors such as SF10–SF12.

## Setting up the sender and receiver

First build both the sender and receiver:
```
./build_project
```

then hold in BOOTSEL on the sender pico, plug it in and flash it:

```
cp build/pico_lora_test_sender.uf2 /media/pi/RP2350
```

and unplug the sender pico.

Hold in BOOTSEL on the receiver pico, plug it in and flash it:

```
cp build/pico_lora_test_receiver.uf2 /media/pi/RP2350
```

and unplug the receiver pico.

## Synchronising WeatherPayload between sender and receiver

The WeatherPayload struct must be the same for the sender and receiver and its size is guarded by **static_assert**s. If they don't match you'll see something similar to:

```
New sender session: 226890954
Invalid weather packet size: 56
```

To work out the size if you add/remove fields you can check the size of each type:

```
arm-none-eabi-g++ -dM -E -x c++ /dev/null | grep SIZEOF
arm-none-eabi-g++ -dM -E -x c++ /dev/null | grep CHAR_BIT
```

and update the **static_assert**s to suit the new packed size.

## Monitoring the LoRa comms between the two picos

Plug in the receiver pico and monitor:

```
picocom /dev/ttyACM0 -b 115200
```

then plug in the sender pico and monitor:

```
picocom /dev/ttyACM1 -b 115200
```

## UK regulations

In the UK, OFCOM state the 433Mhz band transmission requirements in the [Draft UK Interface
Requirement (IR) 2030](https://www.ofcom.org.uk/siteassets/resources/documents/consultations/category-2-6-weeks/consultation-notice-of-proposals-to-make-wireless-telegraphy-regulations-2025/main-docs/draft-ir-2030-2025.pdf?v=409289) (pdf). On Page 8 (accessed 5/10/26) it states the 433.05MHz - 434.79MHz band must have a duty cycle of <= 10% at 10mW effective radiated power (e.r.p.) (IR2030/1/10). A rough guide to **SX1278Config.txPowerDbm** is given below.

|    dBm |            mW |
| -----: | ------------: |
|  0 dBm |          1 mW |
|  3 dBm |          2 mW |
|  6 dBm |          4 mW |
| 10 dBm |         10 mW |
| 13 dBm |         20 mW |
| 17 dBm |         50 mW |
| 20 dBm |        100 mW |
| 30 dBm | 1000 mW = 1 W |

**SX1278Config.txPowerDbm** is set to **10** by default to comply with the ODCOM regulations in the UK.

A worked example, based on the equations in section **4.1.1.7. Time on air** in the [SX1278 datasheet](https://www.semtech.com/products/wireless-rf/lora-connect/sx1278), for the default SX1278Config settings is:

```
Symbol airtime
  Tsym = (2^SF)/BW = (2^7) / 125000 = 0.001024 = 1.024ms

Preamble airtime
LoRa adds 4.25 symbols to the configured preamble length
  Npreamble = (preamble length + 4.25) x Tsym = (8 + 4.25) x 1.024 = 12.544ms

Work out how many payload symbols are required per broadcast
  PL = payload length = 56
  SF = spreading factor = 7
  CRC = 1 as crcEnabled = true
  IH = 0 as assuming explicit header
  DE = 0 as low-data-rate optimisation isn't needed at SF7/BW125
  CR = 1 for coding rate 4/5
  Npayload = 8 + ceil((8PL - 4SF + 28 + 16CRC - 20IH) / (4(SF - 2DE))) x (CR + 4)
    => 8 + ceil((448 - 28 + 28 + 16) / 28) x 5
    => 8 + ceil(464 / 28) x 5
    => 8 + ceil(16.57142857) x 5
    => 8 + (17 x 5)
    => 93 payload symbols required to transmit one weather station packet
    => Tsym = 1.024
    => Tpayload = 95.232ms
    => Tpreamble + Tpayload
    => 12.544 + 95.232 = 107.776ms
    => each broadcast occupies the air for about 108ms
    => 10% duty cycle
    -----------------
```

A 10% duty cycle means that over any representative period, the station may occupy the channel for at most 10% of the time. Over one hour:

```
3600 × (10 / 100) = 360 seconds
```

There is therefore a 360s window each hour the station can transmit, which means the maximum number of 56 byte payload packets per hour is:

```
360 / 0.108 ~= 3333
```

which equates to approximately **3333 broadcasts per hour** assuming an average broadcast airtime of 108ms.

3600 1s blocks in an hour, each one with a broadcast would give:

```
Duty Cycle % = (total transmit airtime / elapsed time) x 100
  => (0.108 x 3600) / 3600 x 100
  => 10.8%
```

Broadcasting every second each hour would be over the 10% limit so we need to reduce the broadcast rate to reduce the duty cycle.

If the station transmits every 10 seconds, with 360 x 10 second blocks in an hour, where during each 10s block there will be 108ms airtime:

```
Duty Cycle % = (total transmit airtime / elapsed time) x 100
  => (0.108 x 360) / 3600 x 100
  => 1.08%
```

Broadcasting 360 packets per hour would be 360 x 108ms ~ broadcasting for 39s per hour.

The [Semtech LoRa Calculator](https://www.semtech.com/design-support/lora-calculator) does all the calculations.

## FreeRTOS-Kernal setup for new projects

When creating a FreeRTOS project from scratch, clone the main branch into the project. The main branch at the moment has the necessary pico functionality:

```
git init
git submodule add https://github.com/FreeRTOS/FreeRTOS-Kernel.git lib/FreeRTOS-Kernel
git submodule update --init --recursive
git add .gitmodules lib/FreeRTOS-Kernel
```

## FreeRTOSConfig.h

This file customises FreeRTOS for your project. The file:

```
include/FreeRTOSConfig.h
```

is this one from the pico-examples:

```
pico-examples/freertos/FreeRTOSConfig_examples_common.h
```

## Interesting

The project uses two pico 2 boards so if you later have a Raspberry Pi, ESP32, or something else decoding the same protocol, make the byte order explicit so the protocol doesn’t depend on CPU endianness.

The example weather station data is in the packet as:

```
temperature            4
humidity               4
pressure               4

windSpeed              4
windGust               4
windDirectionDegrees   2

rainfall               4
lux                    4
batteryVoltage         4
timestamp              4
                       --
                       38 bytes

Plus 6-byte header

44 bytes total
```

## References

* [Task priorites](https://www.freertos.org/Documentation/02-Kernel/02-Kernel-features/01-Tasks-and-co-routines/03-Task-priorities)
* [uxTaskGetStackHighWaterMark](https://www.freertos.org/Documentation/02-Kernel/04-API-references/03-Task-utilities/04-uxTaskGetStackHighWaterMark)
