# CC2510 target firmware

This is a separate PlatformIO/SDCC project. Build from the repository root with `make build-tag`, or run `pio run` here using the activated project virtual environment.

The `cc2510f32-smoke` image leaves peripheral GPIO/RF configuration at reset defaults and increments an XRAM counter. It is a compile/link diagnostic for an independently confirmed CC2510F32, not a display demo and not a generic image for the pictured tags. It does not initialize the crystal, blink a board LED, or implement sleep. Flashing it replaces the running application.

The custom board sets 32,768 bytes of code, 256 bytes of fast IRAM and 3,840 bytes of slow XRAM. `configure.py` sets slow XRAM origin to `0xF000` and code origin to zero. Inspect the generated `.map` and `.mem`; the initialized `etag_ticks` counter should be at `0xF000` in this minimal build. Upper slow RAM has sleep-retention constraints when low-power firmware is introduced.

Output is `.pio/build/cc2510f32-smoke/firmware.hex`, plus map/memory reports. Package it only against a verified profile, using the root tools. The pre-build script rejects `upload`, `uploadfs`, and `program`; PlatformIO's generic serial upload does not implement this chip's TI debug protocol.

For another MCU family, create a sibling project with its actual compiler and board configuration. For another CC2510 capacity, add an accurate board and link configuration; do not change the runtime profile to make an oversized image fit.
