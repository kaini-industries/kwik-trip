# Cardputer and ESP32 application

This is the firmware installed on the **programmer device**. The default build targets the Cardputer Advance and uses its keyboard and display for a portable e-tag workbench. The same application also builds for the two configured ESP32 DevKits with a serial console.

Build from the repository root:

```sh
.venv/bin/pio run -e cardputer-adv
```

The application is generated at `.pio/build/cardputer-adv/firmware.bin`, alongside its bootloader, partition data, and debug ELF. Follow the root [installation instructions](../../README.md#start-with-the-cardputer-advance) to upload with PlatformIO. This folder uses the root `platformio.ini`; it is not an independent PlatformIO project.

On boot, the firmware initializes Cardputer peripherals and opens the workbench menu: 1 IR devices, 2 Wired diagnostics, 3 Browser editor. It waits for an explicit command before probing a tag. A Cardputer Advance runtime board check blocks probing, IR initialization and SD mounting if a different M5 board is detected.

In wired diagnostics mode, the Cardputer keyboard and USB serial port share the same commands. Type `display` to return to the menu. `profiles` lists the compiled catalog, `select <profile-id>` chooses a tag profile, and `probe confirmed` invokes the current wired CC2510 identification/status implementation. `sd` mounts a card; `save` stores the latest probe result as a new JSON log. See [first contact](../../docs/workflows/first-contact.md) for prerequisites and all commands.

The current application can operate without a computer after installation. The computer is still needed to build/install updates, modify the compiled profile catalog, and use the package/backup tools. The Cardputer does not yet read flash, load firmware packages from SD, program tags, or communicate with tags by radio/NFC.

| Source | Responsibility |
| --- | --- |
| [src/main.cpp](src/main.cpp) | Serial/keyboard command handling, screen output, profile selection and SD probe logs |
| [src/display](src/display) | Saved IR devices, text/SD rendering, RMT transmission and USB/BLE bridge |
| [src/sd_card.cpp](src/sd_card.cpp) | Shared SD adapter using generated host pin definitions |
| [src/gpio_wire.cpp](src/gpio_wire.cpp) | TI debug GPIO timing, data direction and reset/release |
| [EtagCore](../../lib/EtagCore) | Portable CC2510 probe and checked transfer utilities |
| [Host definitions](../../config/hosts.json) | Board selection, DD/DC/RESET pins and reserved GPIOs |
| [Tag profiles](../../profiles/README.md) | Evidence-backed identity and hardware metadata |
| [Build hook](../../tools/pio_prepare.py) | Validates metadata and generates the catalog for each environment |

The dispatcher currently constructs a `CcDebugProbe`. To add another protocol, extend dispatch and capabilities, implement its transport, define its required pins, and add simulated transport tests before bench validation. A profile using an unimplemented protocol stays non-operational. See [architecture](../../docs/architecture.md) and [contributing](../../CONTRIBUTING.md).

See [display editing](../../docs/workflows/display-editing.md) for infrared content updates and [bench validation](../../docs/workflows/cardputer-validation.md) for the remaining physical checks. IR updates use stock tag firmware and do not enable flash programming.
