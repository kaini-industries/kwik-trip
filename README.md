# etag

A portable e-tag workbench built around the **M5Stack Cardputer Advance**, with support for other ESP32 development boards. The goal is to investigate, back up, and reprogram a collection of electronic tags using shared tools and a profile for each hardware revision.

**This project already builds firmware for the Cardputer Advance.** It provides keyboard controls, on-screen results, USB serial access, and SD probe logs. Its first implemented tag interface is a **wired TI CC2510 identification/status probe**. Flash readout and reprogramming are upcoming features; wireless tag communication is not implemented.

The project is organized to accommodate different manufacturers, MCU families, and displays. Compatibility is established per hardware revision and operation. The included tag records are initial research examples. No tag model has been physically validated with this firmware yet.

## What runs where

| Component | Runs on | Purpose |
| --- | --- | --- |
| **Cardputer/ESP32 firmware** | Your Cardputer Advance or another configured ESP32 | Provides the user interface and communicates with a matching tag through a wired adapter and implemented protocol |
| **Development tools** | Your computer | Build firmware, validate profiles, prepare image packages, and archive independently acquired backups |
| **Tag firmware** | The electronic tag's own MCU | Controls its display, peripherals, and power management; built separately for that hardware |

The root [PlatformIO project](platformio.ini) builds the Cardputer application from [firmware/programmer](firmware/programmer/README.md). Once installed, you can operate it using the Cardputer's keyboard and screen without a computer. Connecting a tag still requires a matching adapter and independently verified target supply.

The separate [CC2510 target project](firmware/targets/cc2510/README.md) is a compile-only diagnostic example. Other tag MCUs need their own toolchains, memory layouts, and board-specific applications. The Cardputer binary belongs on the Cardputer; each tag needs its own compatible image.

## What works today

| Capability | Current state |
| --- | --- |
| Cardputer Advance and two ESP32 DevKit builds | Implemented and compile-tested |
| Keyboard/display console, USB commands, profile selection, SD logs | Implemented; physical validation pending |
| Wired CC2510 chip identification and debug-lock status | Implemented and tested with a simulated wire; physical validation pending |
| Record additional tag models and revisions | Implemented through validated JSON profiles |
| Package tag images and archive separately acquired dumps | Computer tools implemented; see [format limits](tools/README.md) |
| Read, erase, program, and verify tag flash on the Cardputer | Planned; no commands implement these operations yet |
| SWD, UART bootloader, SPI flash, radio, or NFC communication | No active implementations; some families can be recorded in profiles |
| Tag display control | Requires separate firmware for the tag's hardware |

Adding a profile records a device; it does not implement its protocol. See the [roadmap](docs/plans/roadmap.md) for the path from diagnostics to a portable programmer.

## Start with the Cardputer Advance

Open [etag.code-workspace](etag.code-workspace) in VS Code, or use a terminal in the project root. With Python 3.12 installed:

```sh
python3.12 -m venv .venv
.venv/bin/python -m pip install -r requirements-dev.txt
.venv/bin/pio run -e cardputer-adv
```

The application output is `.pio/build/cardputer-adv/firmware.bin`. PlatformIO also generates the bootloader and partition data. Use its upload command to install the complete build at the correct offsets.

Keep the tag disconnected while installing firmware on the **Cardputer**, and replace `/dev/cu.YOUR_DEVICE` with its actual USB port:

```sh
.venv/bin/pio device list
.venv/bin/pio run -e cardputer-adv -t upload --upload-port /dev/cu.YOUR_DEVICE
```

After boot, type `help` on the Cardputer keyboard. For the optional computer console:

```sh
.venv/bin/pio device monitor --port /dev/cu.YOUR_DEVICE --baud 115200
```

On Windows, create the environment with Python 3.12, activate `.venv\Scripts\Activate.ps1`, and use `python` / `pio` instead of `.venv/bin/` commands. The [build guide](docs/development.md) covers pinned versions and configuring the PlatformIO IDE to use this environment.

## Connect a tag

First follow [adding a tag to your collection](docs/workflows/add-a-tag.md). Identify its MCU, hardware revision, voltage, and debug pads before choosing an interface. The [Cardputer adapter guide](hardware/hosts/cardputer-adv/README.md) describes the current wired CC2510 adapter; other interfaces need their own wiring and implementation.

| Command | What it does |
| --- | --- |
| `help`, `about`, `pins` | Show commands, host details, and configured debug GPIOs |
| `profiles` | List compiled-in tag profiles and the current selection |
| `select <profile-id>` | Select the hardware profile for the connected tag |
| `probe confirmed` | Run the CC2510 identification/status probe after wiring and power checks |
| `sd`, then `save` | Mount the Cardputer SD card and save the latest probe as a new JSON log |

`probe confirmed` means the operator has verified the MCU family, wiring, battery removal, supply voltage, and common ground. It briefly enters debug mode and resets the tag afterward, without changing flash. Unsupported profiles return without activating the debug interface. A saved probe log contains identification results, not a firmware backup. Follow the [first-contact procedure](docs/workflows/first-contact.md) for bench work.

## Bring your own e-tags

Use one profile per model **and hardware revision**, with a separate inventory record for each physical unit. Tags sharing an MCU protocol may still need different wiring, flash capacity, panel drivers, or custom firmware.

Create an unknown profile, replacing `vendor-model-reva` with a useful identifier:

```sh
.venv/bin/python tools/etag.py new-profile vendor-model-reva --output profiles/tags/vendor-model-reva.json
.venv/bin/python tools/etag.py check
```

Add photos and measurements under `hardware/tags/vendor-model-reva/`. Keep unknown fields as `null` and choose a protocol only when evidence supports it. Rebuild and reinstall the Cardputer application when profiles change: profiles are compiled into firmware, and are not loaded from SD at runtime.

The [collection guide](docs/workflows/add-a-tag.md) explains the steps toward supporting another device. [CONTRIBUTING.md](CONTRIBUTING.md) covers extending the software.

## Other hosts and validation

| PlatformIO environment | Host hardware | Interface |
| --- | --- | --- |
| `cardputer-adv` (default) | Cardputer Advance / Stamp-S3A, 8 MB, no PSRAM | Keyboard, screen, USB console, SD logs |
| `esp32-devkit` | Classic ESP32-WROOM DevKit, 4 MB | Serial console |
| `esp32-s3-devkit` | ESP32-S3-DevKitC-1 N8, no PSRAM | Native USB console |
| `native` | Development computer | C++ tests using a simulated debug wire |

Build another host with `.venv/bin/pio run -e esp32-devkit` or `.venv/bin/pio run -e esp32-s3-devkit`. Additional boards and the original Cardputer need their own configuration and verified pin assignments.

Run the complete software checks and firmware build matrix with:

```sh
make validate
```

This validates profiles, reference hashes and documentation links, runs Python/C++ tests, and builds all three ESP32 environments plus the CC2510 example. It does not upload to hardware. The Makefile and supplied editor tasks use macOS/Linux virtual-environment paths; individual commands are in the [build guide](docs/development.md).

## Project guide

| Location | Contents |
| --- | --- |
| [firmware/programmer](firmware/programmer/README.md), [lib/EtagCore](lib/EtagCore) | Cardputer/ESP32 application and portable protocol code |
| [firmware/targets](firmware/targets/README.md) | Firmware projects for the tags themselves |
| [config](config), [profiles](profiles/README.md) | Host definitions, pinned build environments, and tag catalog |
| [hardware](hardware/README.md) | Tag photos, adapter documentation, pad maps, and inventory templates |
| [tools](tools/README.md) | Profile, image package, verification, and backup tools |
| [docs](docs/README.md) | Workflows, architecture, development roadmap, and research references |
| [test](test), [tests](tests) | C++ and Python tests |
| [backups](backups/README.md), [artifacts](artifacts/README.md) | Local acquisitions and generated outputs, ignored by Git |

The original model-specific research and photos remain in the [reference archive](docs/reference/README.md). Their original-file hashes are checked against the [migration manifest](docs/reference/migration-manifest.json).
