# Pico LoRa Test

This is a simple project to send data from one pico to another over LoRa. Each pico is connected to an SX1278 433MHz module.

## Cloning the project

Clone the project with FreeRTOS submodules to get the pico functionality:

```
git clone --recurse-submodules https://github.com/eremiticengineer/pico-lora-test
```

If you cloned without recursing submodules:

```
git submodule update --init --recursive
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

## References

* [Task priorites](https://www.freertos.org/Documentation/02-Kernel/02-Kernel-features/01-Tasks-and-co-routines/03-Task-priorities)
* [uxTaskGetStackHighWaterMark](https://www.freertos.org/Documentation/02-Kernel/04-API-references/03-Task-utilities/04-uxTaskGetStackHighWaterMark)
