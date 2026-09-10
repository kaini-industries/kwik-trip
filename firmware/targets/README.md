# Firmware for electronic tags

Projects here build the programs that run on the **tags themselves**. The [Cardputer/ESP32 programmer application](../programmer/README.md) is built separately from the repository root.

Each target project needs the toolchain and memory map appropriate to its MCU. Each tag hardware revision also needs accurate panel wiring, display initialization, storage layout, and power management. Sharing a manufacturer, case, display size, or MCU family is not sufficient evidence that two tags can use the same image.

| Project | Current scope |
| --- | --- |
| [cc2510](cc2510/README.md) | PlatformIO/SDCC compile-only diagnostic for a confirmed CC2510F32; no panel driver or sleep implementation |

Add sibling projects as hardware is identified. Reuse family-level code where it applies, with explicit board configurations for individual revisions. Keep unknown devices documented in [profiles](../../profiles/README.md) until the required facts are available.

The computer-side [package tools](../../tools/README.md) currently describe a full-capacity main-flash image starting at address zero, padded with `0xFF`. Other address maps, segmented images, bootloader offsets, or protected regions may require an extended package format before a new family can use it. Successful packaging does not prove the Cardputer has a working programming backend for that family.
