# Flashing custom firmware onto a SES-imagotag VUSION 2.2 BWR (CC2510) with an ESP32

Bench guide for the EDG3-0220-B / "GL440" tag. Everything here assumes the MCU is a **TI CC2510** (the chip used across the GL120/GL320/GU140/GL440 family). Confirm the marking when you open the case — a few VUSION variants (the UU340 2.6") use an Axsem AX8052 instead, and none of this applies to those.

---

## 0. The big picture

```
  TAG (CC2510)  ──5 wires──►  ESP32 running ESP_CC_Flasher  ──Wi-Fi──►  browser on your Mac
  DD, DC, RESET_N,             bit-bangs TI's 2-wire debug              http://cc.local
  VDD, GND                     protocol; serves a web UI                read / erase / write .bin

  YOUR MAC:  8051 C source ──sdcc──► main.ihx/.hex ──makebin──► main.bin ──upload──► web UI ──► tag flash
```

Two separate builds are involved. Keep them straight:

1. **ESP_CC_Flasher** — Arduino/PlatformIO project that runs *on the ESP32*. Build and flash it once.
2. **The tag firmware** — 8051 C compiled with **SDCC** on your Mac. Rebuild every iteration; deliver as a `.bin` through the web UI.

The CC2510 debug interface is TI's proprietary 2-wire protocol (Debug Data + Debug Clock, plus RESET). It is not SWD, not JTAG, not UART. ESP_CC_Flasher implements it in software on three GPIOs.

---

## 1. Parts and tools

**Hardware**

| Item | Notes |
|---|---|
| ESP32 board | Zero-friction: any classic ESP32 DevKit (30/38-pin, `esp32doit-devkit-v1` / `esp32dev`, ~$5). The project's pins 23/19/33 exist as shipped. Your S3 boards (Cardputer, Heltec V3) work after a pin remap — see §4. |
| USB data cable | For the ESP32. |
| 5 short leads | 30 AWG wire-wrap wire or fine enamelled magnet wire on the tag end; Dupont pins or a small header on the ESP32 end so you can unplug. |
| Soldering kit | Fine tip, flux, tweezers, loupe/magnifier, Kapton tape or hot glue for strain relief. |
| Multimeter | Continuity beep is the whole game in §3. |
| Optional | Pogo pins + a printed jig if you end up doing several tags. Plastic spudger for the case. |

**Software (Mac)**

- VS Code + PlatformIO (you have this).
- SDCC: `brew install sdcc` — installs `sdcc`, `packihx`, `makebin`.
- Python 3 (for Frickl's `imgconvert.py`), git.

---

## 2. Open the tag and confirm the hardware

1. **Battery out first.** The round "+" on the back shell marks the coin cell. Whether it's a door or just a marking, the cell must be out before you connect anything to the ESP32.
2. Pry the seam with a spudger, working around the perimeter. Expect a tight snap fit or weld. Go slow at the display end — the panel's FPC tail is fragile and the panel is glass.
3. Identify on the PCB: **CC2510** (QFN36, 6×6 mm, marked "CC2510" + "F32"), **NT3H2111** NFC chip, **W25X10CL** 8-pin SPI flash, **TPS61071** boost converter (tiny), the EPD FPC connector, the white LED, the inverted-F antenna trace, and clusters of test pads.
4. **Read the panel part number on the FPC tail** (expect GDEW0213Z16 or an IL0373-compatible equivalent). Write it down — it decides your display init code in §9.
5. **Photograph both sides of the PCB at high resolution** before soldering anything. You'll be zooming in on these later.

If the big chip isn't a CC2510: stop here. An Axsem AX8052 is a different protocol entirely (see BeatSkip/SES-Imagotag-UU340).

---

## 3. Find the five debug pads

You need five nets: **DD** (P2_1), **DC** (P2_2), **RESET_N**, **VDD**, **GND**.

**Reference layout** (one board revision, reported in andrei-tatar/imagotag-hack issue #3 — five pads on each side):

- Front: DVDD · RESET_N · P0_4 · **P2_2 (DC)** · GND
- Back: P0_2 · P0_3 · P0_5 · **P2_1 (DD)** · AVDD

So on that revision DD and DC are on *opposite sides* of the board. Your GL440 may differ — verify with continuity, don't trust the pattern.

**Verify against the chip pins.** CC2510 QFN36: pin 1 is marked by the dot/chamfer; count counter-clockwise looking down at the top. The pins you care about:

| Signal | CC2510 pin |
|---|---|
| P2_1 — Debug Data (DD) | 15 |
| P2_2 — Debug Clock (DC) | 16 |
| RESET_N | 31 |
| DVDD | 2 and 10 |
| GND | exposed pad under the chip; also battery negative |

Pins 15 and 16 sit side by side, which is handy. QFN pins are tiny — beep from the nearest passive (series resistor, decoupling cap) rather than the pin itself if you can trace it.

**Cross-checks that don't need the chip pins:**

- The DD pad also runs to the white LED's series resistor (P2_1 doubles as the LED drive).
- The DC pad also runs to the TPS61071 enable pin (P2_2 doubles as boost enable).
- RESET_N has a pull-up resistor and a small cap right next to the chip.
- The VDD pad beeps to the battery + contact; GND to the battery − contact.

Mark the five pads on one of your photos. You'll refer to it every time you re-wire.

---

## 4. Wire the tag to the ESP32

**Classic ESP32 — as ESP_CC_Flasher ships:**

| Tag pad | Signal | ESP32 pin |
|---|---|---|
| P2_1 | DD — debug data (bidirectional) | GPIO 23 |
| P2_2 | DC — debug clock | GPIO 19 |
| RESET_N | reset (active-low) | GPIO 33 |
| DVDD | +3.3 V | 3V3 |
| GND | ground | GND |

Rules:

- **Coin cell out. The ESP32's 3V3 pin powers the tag.** Never both at once — you'd be tying two supplies together, and 3.3 V logic into a 3.0 V-powered chip is at the edge of the CC2510's input rating anyway.
- **Keep the leads short (≤ 10 cm).** The protocol is bit-banged and TI flags the RESET line as noise-sensitive.
- Solder at the pads, then tape/glue the wires down so a tug pulls on the glue, not the pad.
- **DD and DC are the classic mix-up.** If nothing answers in §6, swap them before anything else.

**ESP32-S3 boards (Cardputer / Heltec V3):**

- GPIO 23 doesn't exist on the S3, and GPIO 19/20 are native USB on the Cardputer.
- Pick three free GPIOs. Avoid strapping pins (0, 3, 45, 46) and 19/20.
- Change the three pin numbers in ESP_CC_Flasher's source (grep `src/` and `include/` for `23`, `19`, `33`).
- In `platformio.ini`, set `board = m5stack-stamps3` (Cardputer) or `board = heltec_wifi_lora_32_V3`.
- Everything else is unchanged.

---

## 5. Build and flash ESP_CC_Flasher (once)

```bash
git clone https://github.com/atc1441/ESP_CC_Flasher.git
cd ESP_CC_Flasher
```

Edit `platformio.ini`:

- Delete `upload_port = COM6` and `monitor_port = COM6` (Windows-only). PlatformIO auto-detects on macOS, or set `/dev/cu.usbserial-*` explicitly.
- Leave `platform = espressif32`. It resolves to the 6.x platform (Arduino core 2.0.x), which is what the shipped async libraries expect. If you're on a pioarduino 3.x core and the build fails, either pin `platform = espressif32@^6` or switch `lib_deps` to the maintained forks `ESP32Async/ESPAsyncWebServer` + `ESP32Async/AsyncTCP`.
- `lib_deps` as shipped: `me-no-dev/ESPAsyncWebServer`, tzapu WiFiManager `feature_asyncwebserver` branch, `WiFiClientSecure`.

Optional: hard-code Wi-Fi credentials in the source. Otherwise WiFiManager opens a captive-portal AP on first boot.

```bash
pio run -t upload
pio device monitor -b 115200
```

First boot: join the ESP's config AP from your phone or Mac, pick your Wi-Fi, save. The serial monitor prints the IP it gets.

**Load the web UI:**

1. Open `http://cc.local/edit` and upload `data/index.htm` to the SPIFFS root.
    - The editor pulls its JavaScript from a CDN — your Mac needs internet, not just the ESP's Wi-Fi.
    - Blank page? SPIFFS isn't formatted. Add `SPIFFS.format();` once near the SPIFFS init in the code, flash, boot once, remove it. (Documented on atc1441's sister project, ESP32_nRF52_SWD.)
    - Alternative: `pio run -t uploadfs` should write the whole `data/` folder to SPIFFS in one shot with the default partition table.
2. Open `http://cc.local` → the flasher UI. If mDNS misbehaves, use the IP from the monitor.

---

## 6. First contact: chip ID, backup, erase

1. ESP32 on USB, tag wired, **no coin cell**.
2. Run the detect / chip-ID action. Expect a **CC2510** (chip ID 0x81; a CC2511 would read 0x91).
3. No ID → go to §10. Nine times out of ten it's DD/DC swapped or RESET not actually connected.
4. **Read flash → save as `original_<serial>.bin`.** It should be 32,768 bytes. Keep it: you can restore the tag, and Ghidra's 8051 loader will happily disassemble it.
    - If the ID works but reads come back empty, garbage, or error out, the chip is **debug-locked**. The lock blocks reads; it does not block erase.
5. **Erase** (full chip erase). This clears the lock and blanks the chip. A read afterwards should be all `0xFF`.

---

## 7. Build the tag firmware (SDCC)

Three starting points, pick one:

**A. angrymew/firmware-cc2510** — simplest build.
`make.sh` compiles each `src/*.c` with `sdcc -mmcs51 -c`, links them, then runs `packihx` to produce `build/main.hex`. Fix one line for macOS:

```bash
sed -i '' 's/SDCC_PATH="sdcc.exe"/SDCC_PATH="sdcc"/' make.sh
chmod +x make.sh && ./make.sh
```

**B. section77/Frickl-EPaper** (mirrored on Codeberg) — andrei-tatar's firmware repackaged as a PlatformIO project on the Intel MCS-51 platform (env `Generic8051`), plus `imgconvert.py` → `image.h` for a compiled-in static image. This is the best "hello world": your picture on the tag.

```bash
pio run
# output: .pio/build/Generic8051/firmware.hex
```

**C. andrei-tatar/imagotag-hack `firmware/`** — the original.

**Convert Intel HEX → raw binary** (the ESP flasher wants `.bin`):

```bash
makebin -p build/main.hex build/main.bin          # ships with SDCC
# alternatives:
#   brew install binutils && gobjcopy -I ihex -O binary main.hex main.bin
#   pip install intelhex && hex2bin.py main.hex main.bin
ls -l build/main.bin                              # must be ≤ 32768 bytes
```

**Panel caveat:** these firmwares assume an IL0373-class panel (GDEW0213Z16, 104×212, on the 2.2"). If your FPC label says otherwise — especially anything SSD1680-class — the init sequence differs. GxEPD2's driver classes contain working sequences for both families to crib from.

---

## 8. Write, verify, run

1. Web UI → select `main.bin` → **Write**. It's bit-banged, so give it time. Verify via the flasher or **Read** back and `cmp` against your file.
2. The flasher releases RESET and the new firmware runs. The white LED (shares the DD pin) may flicker during operations — harmless.
3. When you want the tag standalone: disconnect 3V3 first, then put the coin cell back.

**The iteration loop:** edit → `./make.sh` → `makebin` → upload in the browser → power-cycle. Leave the ESP32 wired up between builds.

---

## 9. Writing your own firmware — what it has to do

Skeleton, in order (all of it exists in the reference repos):

1. **Clock.** Power up the crystal oscillator, wait for the XOSC-stable flag, switch `CLKCON` to the 26 MHz crystal, wait for the switch, power down the RC oscillator. Everything timing-related (SPI to the display, flash writes) assumes 26 MHz.
2. **GPIO per the pin map:**
   `P0_0` EPD power (active-low, via P-FET) · `P0_1` EPD CS · `P0_3` EPD SDI · `P0_5` EPD CLK · `P1_2` EPD D/C · `P1_3` EPD BUSY (input) · `P2_0` EPD RST · `P1_0` powers NFC + SPI flash · `P1_4–P1_7` SPI flash CS/CLK/MOSI/MISO · `P0_4`/`P0_6` I²C SDA/SCL to the NT3H2111 · `P1_1` NFC field-detect · `P2_1` white LED · `P2_2` LED boost enable.
3. **Display refresh** (IL0373 flow): drive `P0_0` low → reset pulse on `P2_0` → power setting `0x01`, booster soft-start `0x06`, power on `0x04` then wait BUSY, panel setting `0x00`, resolution `0x61`, VCOM/data-interval `0x50` → `0x10` + black/white plane → `0x13` + red plane → `0x12` refresh, wait BUSY (several seconds) → `0x02` power off → `0x07 0xA5` deep sleep → `P0_0` high. Bit polarities differ between the two planes; use GxEPD2's `GxEPD2_213c` as the reference.
4. **Memory.** 4 KB RAM total. A 104×212 two-plane frame is 5,512 bytes, so don't buffer it — stream rows straight into the controller. Static images live as `__code` arrays in program flash (5.5 KB each of your 32 KB), or in the 128 KB W25X10CL over SPI for anything bigger.
5. **Sleep.** PM2/PM3 via the `SLEEP` register with the sleep timer for periodic wake. `P1_1` (NFC field detect) as an interrupt gives you a "tap a phone → redraw" trigger for free.
6. **Debug output.** Bit-bang a TX-only UART on `P0_2` (it has a back-side test pad and is otherwise unused) at 9600 baud, into a spare UART RX on the same ESP32. Same 3.3 V rail, same serial monitor.
7. **Radio.** Configure the RF registers per the datasheet — but only other CC25xx parts can hear it. A CC2500 module on your Cardputer is the natural gateway. Leave this for after the display works.

---

## 10. Troubleshooting

| Symptom | Likely cause | What to do |
|---|---|---|
| No chip ID | DD/DC swapped · RESET not connected · VDD/GND open · leads too long · coin cell still in | Swap DD/DC · re-beep RESET_N to pin 31 · check 3V3 at the tag's VDD pad · shorten to ≤ 10 cm · pull the cell |
| Still no ID with everything verified | LED/boost circuit loading the shared DD/DC lines | andrei-tatar's board photos show parts removed. Try lifting the LED's series resistor or the TPS61071 enable line, then retry. |
| ID OK, read empty/garbage/fails | Debug lock | Erase, then write. Original firmware is unrecoverable on a locked chip. |
| Write verify fails | Uploaded `.hex` instead of `.bin` · file > 32 KB · flaky wire · power | `makebin` · check size · reflow joints · check 3V3 under load |
| Flashes fine, display dead | `P0_0` not driven low · no reset pulse · BUSY polarity · wrong panel init · FPC unseated in the connector | Blink the LED (`P2_1`) first as a heartbeat, then work down the display sequence |
| `/edit` page blank | SPIFFS unformatted · no internet for the editor's CDN assets | `SPIFFS.format()` once · get the Mac online |
| `cc.local` doesn't resolve | mDNS | Use the IP from the serial monitor |
| Async libraries won't compile | Arduino core mismatch | Pin `espressif32@^6`, or swap to the ESP32Async forks |
| ESP32 resets/browns out when tag attached | Short on the tag pads | Inspect for solder bridges under magnification |

---

## 11. Links

- ESP_CC_Flasher (ESP32 side): https://github.com/atc1441/ESP_CC_Flasher — walkthrough video linked in its README
- Pin map, datasheets (`doc/cc2510.pdf`, `doc/IL0373.pdf`), programmer, original firmware: https://github.com/andrei-tatar/imagotag-hack
- Debug-pad layout report: https://github.com/andrei-tatar/imagotag-hack/issues/3
- SDCC-based firmware playground: https://github.com/angrymew/firmware-cc2510
- PlatformIO-packaged firmware + image converter: https://codeberg.org/section77/Frickl-EPaper (GitHub mirror archived)
- If your chip turns out to be an AX8052: https://github.com/BeatSkip/SES-Imagotag-UU340
