# Development

Run `make validate` before delivering changes. This runs metadata/reference/link checks, Python tests, native protocol tests, all three ESP32 builds, and the independent CC2510 build. CI performs the same categories on Ubuntu. Host integration still needs a physical bench check.

**Add a tag:**

For a guided collection workflow, start with [adding a tag](docs/workflows/add-a-tag.md). A hardware record, protocol implementation, and validated programming operation are separate pieces of support.

1. Run `.venv/bin/python tools/etag.py new-profile vendor-model --output profiles/tags/vendor-model.json`.
2. Create `hardware/tags/vendor-model/`, add original photos, and copy [the pad-map template](hardware/templates/pad-map.md).
3. Record the MCU, board revision, supply, and continuity measurements. Use `null` for unknown values, not guessed zeroes.
4. Set `protocol` to an established family only when evidence supports it. `candidate` supports investigation; `verified` requires measured hardware evidence and complete MCU/power data.
5. Run `tools/etag.py check`, tests, and all host builds. The generated catalog updates automatically at build time; reinstall the host firmware to make the new profile available on the Cardputer.

**Add an ESP32 host:**

1. Add an environment in `config/platformio/hosts.ini`, inheriting the pinned `esp32` section.
2. Add the same environment ID and PlatformIO board ID to `config/hosts.json`.
3. Record the exact module, flash/PSRAM configuration, pin conflicts, supply source, and connector orientation under `hardware/hosts/<id>/`.
4. Add the environment to Makefile and CI. Verify idle pins, explicit probe, and reset/release behavior on the bench.

Do not use a nearby board definition just to make compilation pass. `m5stack-stamps3` describes the Advance's compatible MCU/flash baseline; M5Cardputer/M5Unified provide Advance peripherals, and a runtime board check blocks probing if a different M5 board is detected.

**Add a protocol:**

Keep target protocol classes in `lib/EtagCore`, with hardware operations behind a small interface. The current dispatcher instantiates `CcDebugProbe`, and the host schema describes DD/DC/RESET pins. Extend dispatch, capabilities, transports, and pin configuration for a new interface. A shared connector does not make TI debug compatible with SWD, SPI, or a serial bootloader. Unsupported metadata protocols currently generate `Protocol::Unknown` and remain non-operational. Add fake transport tests for identification, errors, timeouts, and release before enabling GPIO access.

When adding erase/program, implement exclusive operation ownership, complete package validation, backup state, chip/profile matching, address limits, explicit erase selection, checked status/return values, blank checks, full readback verification, bounded waits, and retained failure state. The current `stageBlock` helper expects distinct input/output buffers. It is tested at transfer boundaries but is not itself a flash writer.

**Dependencies:**

Update versions in PlatformIO configs together with the affected build matrix. Python requirements are constrained by `requirements-lock.txt`. Recreate the virtual environment and freeze a reviewed dependency set when updating it. Do not use floating platform URLs or copy old networking libraries into `lib/`.

No project-wide distribution license has been selected. Keep upstream attribution and check licenses before importing code; see [third-party references](docs/reference/third-party.md).
