# Cardputer Advance flashing plan

Reviewed September 9, 2026. This is an analysis and proposed implementation plan, not a hardware-tested flashing procedure.

Use the Cardputer Advance as a wired programmer, with a small adapter providing regulated target power and three debug signals. Reuse and repair the CC2510 protocol implementation from ESP_CC_Flasher inside a focused Cardputer application. Establish chip identification and backup first, then program a minimal diagnostic firmware, then bring up the display. Add wireless content updates only after wired recovery works.

**What the project establishes**

The workspace initially contained two Markdown notes and two exterior photographs. There was no firmware source, build configuration, backup, PCB photograph, measured pad map, or Git repository. PlatformIO is available on this Mac; `sdcc` was not found on the shell PATH. No firmware was compiled or flashed during this review.

| Evidence | What it establishes | What remains unknown |
| --- | --- | --- |
| `6D99A372-3531-4457-8470-B0A48CB888F9.jpeg` | Label reads VUSION 2.2 BWR GL440, EDG3-0220-B | Actual MCU, PCB revision, panel controller, pad locations, protection state |
| `D0B2DFE9-E5D8-4819-A1C6-937D9060D471.jpeg` | Different housing, apparently marked ERD3-0210-A | Entire internal hardware profile; do not apply the EDG3 procedure automatically |
| Existing notes | Useful CC2510 research and reference links | Their claimed compatibility with these particular units is unverified |

The original [imagotag-hack research](https://github.com/andrei-tatar/imagotag-hack) documents CC2510 hardware in GL120 tags. [angrymew's reference](https://github.com/angrymew/firmware-cc2510) also discusses GL440 display hardware, but this does not identify either photographed PCB. The exact EDG3 FCC filing lists internal photos; retrieval of that PDF failed during this review, so no chip identification is claimed from it.

**Corrections to the existing approach**

- Treat each model and PCB revision as a separate target profile. “GL440” alone is insufficient evidence of the MCU or panel.
- Replace “pick three free Cardputer pins” with an Advance-specific adapter map and a defined power circuit.
- Replace “failed read means debug lock” with an explicit status check. Resolve wiring, power, clock, and transport errors separately.
- Do not make erase the immediate next step after detection. Prepare the replacement firmware and complete the backup decision first.
- Do not swap unknown pads while powered or remove LED/boost components based only on an unsuccessful connection. Confirm nets, then inspect signals if necessary. The [referenced issue](https://github.com/andrei-tatar/imagotag-hack/issues/3) asks about removed components; it does not establish removal as a proven repair.
- A main-flash dump alone is not a complete device backup. Account separately for the information page and any external storage or NFC configuration that is present and accessible.
- Remove the assumption that all suggested starter projects display the correct image without modification.

**Proposed Cardputer adapter**

The following assignment is a design choice using GPIOs exposed by M5Stack. It is conditional on confirming a CC2510 target and its pad map. Remove other expansion modules while using the programmer.

| Function | Advance GPIO / net | Position in M5Stack's EXT pin-map table | Target connection |
| --- | --- | --- | --- |
| Debug data | G4 | 3 | Confirmed DD pad |
| Debug clock | G6 | 5 | Confirmed DC pad |
| Target reset | G15 | 14 | Confirmed RESET_N pad |
| Ground | GND | 4 | Confirmed ground |
| Adapter supply input | 5VOUT | 6 | Regulator input only |
| Target supply | Adapter regulated output | Separate adapter terminal | Confirmed target VDD |

The official EXT header exposes 5VOUT, not a dedicated 3.3 V supply. Use a 5 V-to-3.3 V regulator for a board verified to tolerate 3.3 V, or a separate current-limited bench supply. Measure the output before attaching the tag. Never connect 5VOUT directly to tag VDD. [M5Stack Cardputer-Adv documentation](https://docs.m5stack.com/en/core/Cardputer-Adv)

The schematic's P3 terminal numbering differs from the website table's physical-position numbering. Wire by net labels and the documented connector orientation, not an unqualified “pin 3.” This was checked visually on sheet 4 of the [M5Stack schematic](https://m5stack-doc.oss-cn-shenzhen.aliyuncs.com/1178/Sch_M5CardputerAdv_v1.0_2025_06_20_17_19_58.pdf).

Reserve G8/G9 for the internal I2C devices and G14/G39/G40 for microSD SPI; G5 remains available for a later radio chip select. Avoid G3, a strapping pin, and native USB G19/G20. [M5Stack pin map](https://docs.m5stack.com/en/core/Cardputer-Adv), [Espressif GPIO documentation](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-reference/peripherals/gpio.html)

Use a 2×7 breakout, short leads, labelled connections, and strain relief for the first prototype. Fit a manual target-power switch and regulator decoupling as specified by the chosen regulator. A bench supply with current limiting is useful during initial board investigation. Optional small series resistors on DD/DC can be evaluated with signal measurements; they do not provide voltage translation.

Remove the tag battery before externally powering it. Keep programmer outputs high impedance until target power is valid. Before powering the tag down, release the signal drivers and their pull-ups; disconnect the harness before restoring battery operation. A target operated below 3.3 V needs an appropriately designed interface, not direct 3.3 V GPIO drive by assumption. Start with a verified 3.3 V target to keep the first adapter simple.

**What to reuse, and what to fix**

Public repositories were downloaded to temporary folders for static inspection. The original workspace notes were not changed.

| Reference | Reviewed revision | Recommendation |
| --- | --- | --- |
| atc1441/ESP_CC_Flasher | `c61a4183aaf99ece83ad208a0ab90dfe46fdd24a` | Reuse the small GPIO/debug backend after correcting the issues below |
| andrei-tatar/imagotag-hack | `dc0145f1e48d0131240a57750aaa8514d024423c` | Useful CC2510 HAL, display, and linker references; replace its application entry point |
| section77/Frickl-EPaper, Codeberg | `250758ddd2f9c8650d5385cf9d8f932ab09357e7` | Useful example and jig files; repair before treating it as a starter firmware |

Specific flasher findings:

- `src/main.cpp:45,51` ignores erase and programming return values. The function resets the target even after all verification attempts fail (`:65`). Failed operations should leave an explicit error state and should not start suspect firmware.
- `src/cc_interface.cpp:201` always reads 64 bytes from the source buffer. The allocation is exactly the file length, so the final transfer can overread it. Odd-length files are also rounded up without allocating or initializing the extra byte. Use a separate 64-byte staging buffer filled with `0xFF`, copy only available bytes, and verify the defined padded range.
- `src/web.cpp` validates that a firmware file is nonempty but does not enforce a detected target capacity. Add target/profile, address, length, file-read, and hash checks before erasing.
- Flash operations in the main loop coexist with web handlers that can issue debug commands. Give one worker exclusive ownership of the target; queue requests and reject conflicting actions.
- `clock_init()` changes CLKCON, but the normal programming path does not explicitly set FWT for that clock. Make target clock and flash-write timing explicit and propagate clock failures. Re-establish programming state after unlocking.
- The repository vendors old networking libraries under `lib/`; changing `lib_deps` alone is not a complete migration. For the first Cardputer version, leave the web layer out and use screen, keyboard, USB serial, and microSD.

These are source-level findings, not reproduced failures on this user's hardware. See the reviewed [main application](https://github.com/atc1441/ESP_CC_Flasher/blob/c61a4183aaf99ece83ad208a0ab90dfe46fdd24a/src/main.cpp), [debug backend](https://github.com/atc1441/ESP_CC_Flasher/blob/c61a4183aaf99ece83ad208a0ab90dfe46fdd24a/src/cc_interface.cpp), and [web handlers](https://github.com/atc1441/ESP_CC_Flasher/blob/c61a4183aaf99ece83ad208a0ab90dfe46fdd24a/src/web.cpp).

Specific tag-firmware findings:

- Frickl's `src/display/epd.c:26–27` selects 152×296, while 104×212 is commented out. Its image loop uses `imageN[y][x-1]` with `x` starting at zero (`:62,69`), producing an out-of-bounds access and missing the last byte of each row. Correct the indexing, regenerate images for the confirmed panel, and bound BUSY waits.
- Frickl's `main.c` does not explicitly initialize the crystal clock and ends with continuous LED blinking. Its generic build configuration also needs review against the real CC2510 memory layout. It is a demonstration, not a finished coin-cell application. [Reviewed Frickl source](https://codeberg.org/section77/Frickl-EPaper/src/commit/250758ddd2f9c8650d5385cf9d8f932ab09357e7/src/display/epd.c)
- The original imagotag-hack `main.c` has `epd_init()` commented out. Its UART uses P0_4/P0_5, which overlap NFC/display nets in the reference map. Do not simply enable the display while retaining that UART setup. [Application](https://github.com/andrei-tatar/imagotag-hack/blob/dc0145f1e48d0131240a57750aaa8514d024423c/firmware/src/main.c), [UART setup](https://github.com/andrei-tatar/imagotag-hack/blob/dc0145f1e48d0131240a57750aaa8514d024423c/firmware/src/hal/uart.c)

**Implementation milestones and acceptance checks**

1. **Identify one tag of each model.** Open with battery disconnected. Record MCU marking including capacity suffix, PCB revision, FPC label, panel connections, and supply measurements. Photograph both PCB sides and annotate continuity measurements. Complete the first profile on the EDG3-0220-B if it matches the reference hardware. An alternative MCU requires a separate programmer backend and target firmware.

2. **Build a Cardputer diagnostic application.** Use Arduino/PlatformIO and M5Cardputer, starting from M5Stack's Advance configuration. Verify display, keyboard, USB serial, SD, and idle GPIO behavior; then freeze exact dependency versions. Add only enter-debug, identify, status, dump, and release/reset initially. Show the raw response as well as its interpretation. Acceptance: stable identification over ten attempts and several target power cycles; missing target produces a bounded error, never an erase.

3. **Make and validate the backup.** Read accessible main flash twice, independently, and compare hashes and bytes. Save chip identity, raw status, hardware profile, read length, and tool revision alongside the files. Save accessible information-page and external-memory data separately. Restore the normal memory selection after every read, including errors. Acceptance: repeatable reads, or a positively identified protection state recorded as “original firmware unavailable.” Unexplained garbage is a transport failure to investigate.

4. **Build a minimal tag diagnostic.** Use a fresh entry point with explicit clock setup, verified GPIOs, display power disabled, and a bounded visible heartbeat or optional UART on a verified unused pad. Validate linker output and reset-vector placement. Inspect RAM allocation rather than relying only on a successful compile. Build the display demo separately so a panel problem cannot masquerade as a programming problem.

5. **Enable deliberate erase/program/verify.** Stage the entire image before touching flash. Require a matching target profile, nonzero valid length, known address range, and correct hash. Present backup status and the selected image before a deliberate erase action. Check erase completion, refresh status, establish the programming clock, blank-check, program, and compare a complete readback. On failure, stop with the failing address/status; do not automatically run or repeatedly rewrite. Acceptance: exact verification and a diagnostic that starts after a cold power cycle.

6. **Bring up the display, then sleep.** Begin with a generated border and black/red test areas, avoiding imported image conversion as another variable. Verify orientation, dimensions, plane polarity, BUSY behavior, refresh, and power-off. For a confirmed 104×212 two-plane panel, the raw image is 5,512 bytes; stream from code or external storage instead of allocating a full frame. Add low-power operation only after the display works. Acceptance: a correct image survives full power removal; disconnected-programmer current measurements demonstrate that the MCU, LED boost, and display have entered their intended idle states.

7. **Make a repeatable jig.** Once pad positions are measured, build a keyed pogo fixture that supports the PCB without loading the glass or FPC. Allow for pads on both sides. Add an on-device file picker, per-unit backup naming, clear contact/lock/failure states, and a log for each programming operation. Acceptance: repeat the entire workflow on another unit of the same verified revision without changing wiring or source code.

Essential TI-specific checks: CC2510 DD is P2_1/pin 15, DC is P2_2/pin 16, and RESET_N is pin 31. GET_CHIP_ID returns model and revision; model `0x81` identifies CC2510 but not its flash capacity. READ_STATUS bit `0x04` reports debug lock. Erase clears protection and destroys the original firmware; refresh lock status afterward. For F32, use CODE `0x0000–0x7FFF`, IRAM 256 bytes, and slow XRAM `0xF000–0xFEFF`; account for nonretained sleep memory. Set FWT for the selected clock; TI's 26 MHz example uses `0x2A`. [TI CC2510 datasheet, sections 7, 10–12](https://www.ti.com/lit/ds/symlink/cc2510.pdf)

**Suggested project organization**

```text
hardware/                    measured photos, pad maps, adapter, jig
profiles/                    MCU, capacity, PCB revision, panel, power
firmware/cardputer/           UI, storage, target workflow, debug backend
firmware/tag-cc2510/          diagnostic and display applications
tools/                       image preparation and firmware packaging
backups/<unit>/               original dumps, metadata, hashes
tests/                       transfer bounds and workflow failure cases
```

Maintain separate artifacts for the Cardputer and tag. A tag package should include its binary and a small manifest identifying MCU, capacity, board/panel profile, load address, actual/padded lengths, source revision, and SHA-256. Fill sparse HEX gaps with `0xFF` and preserve address zero. A canonical full-capacity binary simplifies whole-flash verification; only use 32,768 bytes after confirming F32. Do not write backup lock settings back automatically during development.

Useful software checks include 1/63/64/65-byte transfer boundaries, maximum-size images, oversized/mismatched packages, short file reads, erase timeout, verify mismatch, and target loss. These test the defects and failure paths identified above. Physical signal timing and power sequencing still require a bench check.

**Wireless updates are a later feature**

The initial custom-firmware installation uses the wired debug connection. A future CC2500 module is a plausible RF gateway for a confirmed CC2510 tag; TI documents [CC2500 compatibility with CC2510](https://www.ti.com/product/CC2500). Matching radio parameters and a shared packet protocol are still required. The Cardputer's native Wi-Fi/BLE or IR does not automatically speak that protocol.

After the display milestone, prefer image/data updates into external flash with addressing, chunk checksums, acknowledgements, and a commit step. Keep the last complete image until the new one validates. Over-the-air firmware replacement additionally needs a recoverable bootloader and a flash-layout/power-loss design; a CC2500 module alone does not provide it. NFC updates likewise need verified NFC hardware and application code, not merely a phone tap.

[TagTinker ADV](https://github.com/i12bp8/tagtinker-adv) targets compatible infrared labels and explicitly excludes radio-only labels. It can inform a later editor UI but is not the initial flasher. The [newer CC-Tool Pro fork](https://github.com/z9m/ESP32-CC-Tool-Pro) advertises useful UI/debug features, but its supported-chip list does not explicitly establish CC2510 support; assess its backend before selecting it.

The immediate next deliverable should be a Cardputer application that reliably identifies and backs up one physically verified tag. That resolves the highest-risk unknowns before display firmware, RF hardware, or a production jig becomes a dependency.
