# Build environment

Use the project-local Python 3.12 virtual environment. A global PlatformIO installation on this Mac runs Python 3.14; discovery of another installed platform caused that interpreter to exit during library resolution. Keeping the workspace environment explicit avoids that dependency on the system interpreter.

| Component | Pin |
| --- | --- |
| PlatformIO Core | 6.2.0 |
| Espressif platform | 6.12.0 |
| Arduino ESP32 framework package | 3.20017.241212 (Arduino 2.0.17) |
| ESP image tool and Python dependencies | esptool 5.4.0, constrained requirements |
| M5Cardputer | commit `2d4fa6646e4e5b47e0af96214b003aa7b15b8d81` (upstream tag 1.2.0; manifest still reports 1.1.1) |
| M5Unified / M5GFX | 0.2.21 / 0.2.28 |
| Display protocol source | TagTinker ADV commit `fb8a0669bbdfe77c51bdbd5adbb8cb2ab00f2db2`, adapted locally |
| Browser test runtime | Node.js 22.14.0 in CI; Node 22+ locally |
| Native platform / Unity | 1.2.1 / 2.6.1 |
| Intel MCS51 platform / SDCC package | 2.2.0 / 1.40100.12072 (SDCC 4.1.0, revision 12072) |

PlatformIO's ESP image generator imports Python dependencies such as `intelhex`; these are supplied by the pinned `esptool` requirement, including when the global package cache was populated by a different interpreter. Platform and tool packages use PlatformIO's user cache; compiled objects and build outputs stay under this checkout's ignored `.pio/` directory. The project-local build cache also keeps SCons' signature database outside the per-environment directory so dependency discovery cannot remove it during a build. No global package files need manual editing.

The CC2510 example pins the same SDCC release on Linux x86_64, macOS, and Windows. The registry's newer `1.40400.0` package has no Linux distribution, so a build using that pin can pass on macOS and fail before compilation on GitHub's Linux runner. Check the [official package's platform coverage](https://registry.platformio.org/tools/platformio/toolchain-sdcc) before changing it; a newer package number alone does not establish cross-platform availability.

Native tests retain Unity's runner through [PlatformIO's custom runner extension](https://docs.platformio.org/en/latest/advanced/unit-testing/frameworks/custom/examples/custom_unity_library.html), disabling its additional floating dependency so the exact `lib_deps` pin controls the test library.

The supplied VS Code tasks call the local virtual environment directly. To use the same Core from PlatformIO's toolbar, set `platformio-ide.useBuiltinPIOCore` to `false` and prepend the absolute path to this checkout's `.venv/bin` to `platformio-ide.customPATH` in your local editor settings. Preserve the remaining system PATH for Git and compilers. On Windows use `.venv\\Scripts` and the Windows PATH separator. These machine-specific settings are intentionally not committed. See [PlatformIO's IDE settings](https://docs.platformio.org/en/latest/integration/ide/vscode.html#settings).

For local port settings, create ignored `config/platformio/local.ini`:

```ini
[env:cardputer-adv]
upload_port = /dev/cu.YOUR_DEVICE
monitor_port = /dev/cu.YOUR_DEVICE
```

The ordered `extra_configs` list loads this optional file after the committed definitions, so overrides do not depend on filesystem enumeration order. Keep the same environment name and board/pin profile; new boards get new committed environments. There is no Wi-Fi service; the browser editor connects over USB or BLE.

`make validate` is the broad check. Use `make test`, `make build-hosts`, or `make build-tag` while iterating. Build/test never uploads firmware. The CC2510 pre-build script rejects upload targets even if requested, because the generic serial uploader does not speak TI debug.

To run the same checks individually, activate the virtual environment with `source .venv/bin/activate` on macOS/Linux or `.venv\Scripts\Activate.ps1` in Windows PowerShell, then run these from the repository root:

```sh
python tools/etag.py check
python -m unittest discover -s tests -v
pio test -e native
python tools/test_display.py
pio run -e cardputer-adv -e esp32-devkit -e esp32-s3-devkit
pio run -d firmware/targets/cc2510
```

The root project creates firmware for the Cardputer/ESP32. The final command creates the separate tag diagnostic image. See [the application guide](../firmware/programmer/README.md) and [target projects](../firmware/targets/README.md) for their distinct roles.

A successful ESP32 compile checks toolchain/library compatibility, not keyboard, SD, voltage, reset timing, or tag communication. Record physical validation in the per-host and per-tag hardware notes. CI artifacts distinguish `programmer-*` binaries from `cc2510f32-smoke-compile-only` HEX files.

The [GitHub Actions workflow](../.github/workflows/ci.yml) first validates the catalog and runs Python/native tests, then builds all three ESP32 hosts and the independent CC2510 example. It never uploads to connected devices. Python is selected from `.python-version`; the pip cache explicitly hashes both `requirements-dev.txt` and `requirements-lock.txt`, since the default cache lookup expects a differently named requirements file. Actions use pinned release commits; the display tests explicitly select Node 22.14.0.

Artifact uploads explicitly include the selected files inside `.pio` and fail if no outputs are found. The `programmer-*` artifacts contain application binaries/debug ELFs; `cardputer-adv-flashable` additionally contains complete factory and Launcher application images with checksums and a manifest. The CC2510 artifact contains diagnostic HEX/map files. The [committed Cardputer downloads](../firmware/releases/cardputer-adv/README.md) remain available directly in Git even when Actions artifacts expire. CI success establishes software/build compatibility; physical tag support is recorded separately.

`tools/test_display.py` compiles the imported frame/codec/waveform suites and persistence/input-boundary integration tests with AddressSanitizer and UBSan, then runs the browser codec and upload tests. Use macOS/Linux (or WSL) with a C++17 compiler for this sanitizer suite. It is part of `make validate` and CI. `make studio` serves `web/` on localhost only, without credentials or external hosting.

## Refresh the committed Cardputer binaries

`make package-cardputer` builds and verifies a local package, including during development. `make release-cardputer` does a clean build, creates the package, then copies only the two binaries, manifest and checksums to `firmware/releases/cardputer-adv/`. It never flashes hardware or publishes to a service.

1. Remove `config/platformio/local.ini` before packaging; local overrides are rejected for distribution builds.
2. Commit firmware/build-input changes locally first. The release manifest records that source commit, and promotion rejects packages built from modified or untracked build inputs.
3. Run `make release-cardputer`, then `make validate`.
4. Commit the updated files in `firmware/releases/cardputer-adv/`, then push both commits. CI rejects stale binaries when build inputs change without refreshing the downloads.

On Windows with the virtual environment activated, the equivalent commands are `pio run -e cardputer-adv -t clean`, `pio run -e cardputer-adv -t package`, `python tools/cardputer_release.py publish`, and `python tools/cardputer_release.py verify`. The `publish` subcommand only copies files into this checkout; it makes no network requests.

Build-input fingerprints cover application/core sources, host/tag metadata, dependency locks, PlatformIO configuration and packaging hooks. Documentation-only edits do not force a rebuild. For offline integrity/source checks use `make verify-release`; no PlatformIO build cache is needed. Image verification uses pinned esptool 5.4.0's parser and explicitly checks its checksum and digest results. Tests exercise corrupted images, wrong offsets, canonical release metadata, exact image merging and stale source detection.

The factory image must stay compatible with M5Burner 3 and zero-offset ESP32 flashing. The separate app image must remain suitable for Launcher's SD/WebUI installer without replacing its bootloader or hard-coding its app partition. Review the [installation guide](../firmware/releases/cardputer-adv/README.md) whenever the partition scheme or hardware changes.
