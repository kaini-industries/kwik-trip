# Cardputer Advance validation

## Automated checks

Run `make validate` from the project root with the documented Python environment, a C++17 compiler supporting AddressSanitizer/UBSan, and Node.js 22 or newer. The build matrix includes Cardputer Advance, ESP32 DevKit, ESP32-S3 DevKit and the separate CC2510 diagnostic example. These commands do not upload firmware or open a target programming interface.

The display suite exercises protocol frame vectors, CRC/waveform timing, minimum wake duration, image codec round trips and size limits, browser upload acknowledgements, capability separation, bounded-line rejection/recovery, and saved-device persistence with failed writes and corrupt storage. The existing fake-wire TI tests and package/profile tests remain active.

The Cardputer build uses Stamp-S3's 8 MB/no-PSRAM configuration and the pinned M5 libraries. Runtime detection must identify `board_M5CardputerADV` before enabling IR or SD. Generated pin validation requires IR TX 44 and SD CLK/MISO/MOSI/CS 40/39/14/12, matching the Advance schematic. RMT uses channel 0 and four TX memory blocks; do not add a competing RMT user without revisiting that allocation.

## Physical checks still required

No Cardputer was connected during integration. Compilation, static RAM/flash size, software tests and browser inspection cannot establish keyboard operation, dynamic heap margin, IR range or tag compatibility. Record the following results per host and tag revision before marking physical support verified.

1. **Boot and navigation:** install the Cardputer image with PlatformIO, with any wired tag disconnected. Confirm all three menu choices, keyboard entry, Backspace navigation and display rotation. Enter the wired console and return with `display` repeatedly.
2. **Saved devices:** add a known-compatible tag, name it, restart and confirm its address/settings. Rename, change orientation/palette, restart, and remove the record. Confirm the wired profile selection is independent.
3. **Text/IR:** send a short monochrome pattern/text to a compatible powered tag at close range in Reliable mode. Check orientation, colors and destination page visually. Repeat, cancel partway through an update, then repeat successfully. Record tag model, PCB revision, barcode type code, distance and firmware commit. Emission alone is not a pass.
4. **SD:** insert a FAT32 card with small PNG/JPEG/BMP/QOI examples under `/etag/images`. Exercise selection, rendering, missing card, retry, and shared access after returning from the console. Existing files must remain intact. Test larger panels with simpler images first.
5. **USB browser:** open the Cardputer USB browser mode, connect the local editor, save a tag and stage an image. Confirm staging alone does not change the tag. Send explicitly and verify the display. Disconnect/reconnect, retry a cancelled/failed upload, and confirm the console is available after exiting browser mode.
6. **BLE browser:** reboot and repeat the browser workflow over BLE. Cycle browser mode and reconnect repeatedly. Record free/largest heap if troubleshooting; a successful build's static RAM number excludes BLE and image buffers. Exit the mode and confirm advertising stops. Reboot into USB if large images run out of memory.
7. **Wired regression:** after the independent voltage/wiring checks in [first contact](first-contact.md), confirm the known CC2510 probe still returns repeatable ID/status and saves an SD probe log. This check is separate from IR compatibility and does not test flash programming.

Preserve logs and measurements under the appropriate `hardware/hosts/` and `hardware/tags/` records. Do not convert an upstream report or a passing simulated test into a local hardware-verification claim.
