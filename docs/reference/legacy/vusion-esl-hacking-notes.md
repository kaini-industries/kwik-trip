# SES-imagotag VUSION 2.2 BWR (GL440) — hacking notes

Condensed summary of everything covered. The full step-by-step is in `vusion-cc2510-esp32-flashing-guide.md` / `.pdf`.

## The device

- **VUSION 2.2 BWR GL440**, model EDG3-0220-B, FCC ID 2ACQM-EDG3-0220-B (approved Dec 2020; internal photos on file, schematics/BOM confidential).
- 2.13" black/white/red e-paper, 2.4 GHz proprietary radio, NFC, white LED. "GL440" is a housing variant, not an electronics generation.
- **Inside (CC2510 family, verify on opening):** TI **CC2510** (8051 + 2.4 GHz radio, 32 KB flash, 4 KB RAM) · NT3H2111 NFC (I²C + field-detect) · GDEW0213Z16 or similar IL0373-class panel · W25X10CL 1 Mbit SPI flash · TPS61071 boost for the LED.
- **Caveat:** some 2.6 BWR variants (UU340) use an Axsem AX8052 — different protocol entirely (see BeatSkip/SES-Imagotag-UU340).

## Pin map (from andrei-tatar/imagotag-hack)

| CC2510 pin | Function |
|---|---|
| P0_0 | EPD power enable (active-low, P-FET) |
| P0_1 / P0_3 / P0_5 | EPD CS / SDI / CLK |
| P1_2 / P1_3 / P2_0 | EPD D/C / BUSY / RESET |
| P0_4 / P0_6 / P1_1 | NFC SDA / SCL / field-detect |
| P1_0 | Power to NFC chip + SPI flash |
| P1_4–P1_7 | SPI flash CS / CLK / MOSI / MISO |
| P2_1 | **Debug Data (DD)** — shared with white LED |
| P2_2 | **Debug Clock (DC)** — shared with LED boost enable |
| P0_2 | Spare test pad ("debug port") — good for a bit-banged debug UART |

Chip pins (QFN36, pin 1 = dot, count counter-clockwise): P2_1 = 15, P2_2 = 16, RESET_N = 31, DVDD = 2 & 10, GND = center pad. One board revision has 5 test pads per side — front: DVDD, RESET_N, P0_4, P2_2, GND; back: P0_2, P0_3, P0_5, P2_1, AVDD.

## Ways to reprogram it

**Path A — reflash the CC2510 (tag stays intact).** Recommended.
- TI 2-wire debug protocol: DD + DC + RESET_N, plus VDD and GND. Not SWD/JTAG/UART.
- A debug-locked chip still accepts chip erase (which clears the lock); you just can't dump the original firmware.
- Toolchain: SDCC (`brew install sdcc`). Starting firmware: angrymew/firmware-cc2510 (make.sh), section77/Frickl-EPaper (PlatformIO, image → `image.h`), andrei-tatar/imagotag-hack.
- Constraints: 32 KB flash, 4 KB RAM. A 104×212 two-plane frame is 5.5 KB → stream to the IL0373 or stage in the SPI flash.
- Radio: proprietary 2.4 GHz FSK, only CC2500/2510/2511 parts can hear it — a ~$3 CC2500 module on an ESP32/Cardputer is the gateway. Not BLE/Wi-Fi/802.15.4. NFC (phone writes → CC2510 wakes on field-detect → redraw) is a radio-free update path.

**Path B — bypass the MCU, drive the panel from an ESP32.**
- Either desolder the CC2510 and wire ESP32 GPIOs to the CS/SDI/CLK/DC/BUSY/RST/power-enable nets (reuses the PCB's booster passives — atc1441 did this on a 4.4" tag), or pull the 24-pin FPC into a Good Display DESPI-C02 / FPC breakout (supplies the booster parts).
- Software: GxEPD2 (`GxEPD2_213c` for GDEW0213Z16); there are also GxEPD2 classes for VUSION/Pervasive panels. Fits an ESP32-S3 + PlatformIO workflow; loses the coin-cell radio form factor.

**Not viable:** VUSION access point + licensed cloud software; OpenEPaperLink (no CC2510 support, no 802.15.4 radio).

## TagTinker ADV (i12bp8/tagtinker-adv)

- Cardputer ADV firmware for **infrared** Pricer tags (SmartTag HD, PP4/PP16, based on furrtek's PrecIR). README states radio-only labels are unsupported → **cannot touch this tag**.
- Still useful as: a no-solder ESL on-ramp if you buy Pricer SmartTag HD tags (targets the ADV; original Cardputer would need a port), and as a template for a Cardputer gateway (Web Studio dithering/planes, Web Bluetooth/Serial transfer, saved targets) with the IR layer swapped for CC2500. GPL-3.0-only.

## Programmers

**CC Debugger (hardware, optional but handy).**
- TI CC-DEBUGGER (~$50) or a clone (~$10–20). Amazon clones: Acxico https://www.amazon.com/dp/B08222729N · kit https://www.amazon.com/dp/B09DYZBSJQ · https://www.amazon.com/dp/B0C5QWM2XG. Skip bundles that include a CC2531 sniffer dongle. Check the kit includes the 10-pin adapter board and a mini-USB cable.
- 10-pin 2×5 header: 1 GND · 2 Target Voltage Sense · 3 DC · 4 DD · 7 RESETn · 9 = 3.3 V out. Pin 2 must see the target's VDD; if powering from pin 9, tie 2↔9; if the tag has its own battery, leave 9 disconnected.
- Mac software: **cc-tool** (dashesy/cc-tool; supports CC2510; build with Homebrew libusb/boost/autoconf/libtool — recipe on the Zigbee2MQTT CC2531 flashing page). `cc-tool -t` detect · `-r file.bin` read · `--erase -w fw.hex` erase+write. Windows: SmartRF Flash Programmer v1 (not v2).
- LED red = no target, green = target detected. Old clone firmware can be updated with SmartRF Flash Programmer.

**ESP32 (software, no purchase).**
- **atc1441/ESP_CC_Flasher** — reads/writes CC1110, CC2430/31 (≤64 kB), **CC2510**, CC2511 via web UI at `http://cc.local`. Wiring (classic ESP32): DD→GPIO 23, DC→GPIO 19, RESET→GPIO 33, GND, tag VDD→3V3 (coin cell out). PlatformIO + Arduino core 2.x (me-no-dev async libs; use ESP32Async forks if the build fails). Upload `data/index.htm` via `/edit` (needs internet; blank page → `SPIFFS.format()` once).
- On ESP32-S3 (Cardputer / Heltec V3): remap the three pins (no GPIO 23 on S3; 19/20 are Cardputer USB), set `board = m5stack-stamps3` or `heltec_wifi_lora_32_V3`.
- Proven pipeline: Frickl-EPaper builds with PlatformIO Generic8051 → hex → `makebin -p main.hex main.bin` → upload with ESP CC Flasher.
- Lesser alternatives: wavesoft CCLib (CC254x/253x-oriented, CC2510 unverified), RedBearLab CCLoader (CC254x only), serisman/cc.flash (Arduino + Windows host), Eric M. Klaus's Arduino Uno CC2510 programmer (archived in imagotag-hack/doc).

## Flashing workflow (short form)

1. Battery out → open case → confirm CC2510 → read panel part number on the FPC → photograph PCB.
2. Continuity-map five pads: DD, DC, RESET_N, VDD, GND.
3. Wire to ESP32 (≤10 cm leads); ESP powers the tag.
4. Build/flash ESP_CC_Flasher; load web UI.
5. Chip ID (expect CC2510, 0x81) → read backup (32,768 B) → erase.
6. Build tag firmware with SDCC → `makebin` → `.bin` ≤ 32 KB → write → verify → power-cycle.
7. Own firmware must: set 26 MHz XOSC clock; configure GPIO per pin map; drive the IL0373 sequence (power on → 0x10 B/W plane → 0x13 red plane → 0x12 refresh → wait BUSY → power off/deep sleep); stream rows (4 KB RAM); sleep via PM2/PM3; optional debug UART on P0_2.

**Troubleshooting:** no chip ID → swap DD/DC, check RESET, VDD, wire length, battery out; still nothing → LED/boost circuit may be loading DD/DC (andrei-tatar's board had parts removed); ID OK but reads fail → debug lock, erase then write; display dead → check P0_0 low, reset pulse, BUSY polarity, panel init.

## Links

- https://github.com/andrei-tatar/imagotag-hack (pin map, datasheets, issue #3 pad layout)
- https://github.com/angrymew/firmware-cc2510
- https://codeberg.org/section77/Frickl-EPaper
- https://github.com/atc1441/ESP_CC_Flasher
- https://github.com/dashesy/cc-tool
- https://blog.jirkabalhar.cz/2023/12/hacking-sesimagotag-e-ink-price-tag/
- https://github.com/BeatSkip/SES-Imagotag-UU340
- https://github.com/i12bp8/tagtinker-adv · https://www.furrtek.org/?a=esl
- https://www.ti.com/tool/CC-DEBUGGER
