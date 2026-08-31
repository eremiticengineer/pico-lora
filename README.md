# Pico LoRa Test

This is a simple project to send data from one pico to another over LoRa. Each pico is connected to an SX1278 433MHz module. The project has a simple text message format but also has a more complex protocol with header, payload type and payload as this is a test harness for my remote, solar powered weather station. It also collects average, minimum and maximum RSSI and SNR as well as detecting lost packets so I can test the range.

## Cloning the project

Clone the project with FreeRTOS submodules to get the pico functionality:

```
git clone --recurse-submodules https://github.com/eremiticengineer/pico-lora-test
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
| Frequency        | 433.000 MHz    |
| Bandwidth        | 125 kHz        |
| Spreading Factor | SF7            |
| Coding Rate      | 4/5            |
| Header           | HeaderExplicit |
| CRC              | Enabled        |
| Preamble         | 8 symbols      |
| Sync word        | 0x12           |
| TX power         | 17 dBm         |

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

## Monitoring the LoRa comms between the two picos

Plug in the receiver pico and monitor:

```
picocom /dev/ttyACM0 -b 115200
```

then plug in the sender pico and monitor:

```
picocom /dev/ttyACM1 -b 115200
```

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
