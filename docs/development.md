# Build environment

Use the project-local Python 3.12 virtual environment. A global PlatformIO installation on this Mac runs Python 3.14; discovery of another installed platform caused that interpreter to exit during library resolution. Keeping the workspace environment explicit avoids that dependency on the system interpreter.

| Component | Pin |
| --- | --- |
| PlatformIO Core | 6.1.19 |
| Espressif platform | 6.12.0 |
| Arduino ESP32 framework package | 3.20017.241212 (Arduino 2.0.17) |
| ESP image tool and Python dependencies | esptool 4.9.0, constrained requirements |
| M5Cardputer | commit `2d4fa6646e4e5b47e0af96214b003aa7b15b8d81` (upstream tag 1.2.0; manifest still reports 1.1.1) |
| M5Unified / M5GFX | 0.2.21 / 0.2.28 |
| IRremote transitive dependency | 4.4.1; no IR feature enabled |
| Native platform / Unity | 1.2.1 / 2.6.1 |
| Intel MCS51 platform / SDCC package | 2.2.0 / 1.40100.12072 (SDCC 4.1.0, revision 12072) |

PlatformIO's ESP image generator imports Python dependencies such as `intelhex`; these are supplied by the pinned `esptool` requirement, including when the global package cache was populated by a different interpreter. The PlatformIO cache is shared by default; source, build outputs and virtual environment are local. No global package files need manual editing.

The CC2510 example pins the same SDCC release on Linux x86_64, macOS, and Windows. The registry's newer `1.40400.0` package has no Linux distribution, so a build using that pin can pass on macOS and fail before compilation on GitHub's Linux runner. Check the [official package's platform coverage](https://registry.platformio.org/tools/platformio/toolchain-sdcc) before changing it; a newer package number alone does not establish cross-platform availability.

Native tests retain Unity's runner through [PlatformIO's custom runner extension](https://docs.platformio.org/en/latest/advanced/unit-testing/frameworks/custom/examples/custom_unity_library.html), disabling its additional floating dependency so the exact `lib_deps` pin controls the test library.

The supplied VS Code tasks call the local virtual environment directly. To use the same Core from PlatformIO's toolbar, set `platformio-ide.useBuiltinPIOCore` to `false` and prepend the absolute path to this checkout's `.venv/bin` to `platformio-ide.customPATH` in your local editor settings. Preserve the remaining system PATH for Git and compilers. On Windows use `.venv\\Scripts` and the Windows PATH separator. These machine-specific settings are intentionally not committed. See [PlatformIO's IDE settings](https://docs.platformio.org/en/latest/integration/ide/vscode.html#settings).

For local port settings, create ignored `config/platformio/local.ini`:

```ini
[env:cardputer-adv]
upload_port = /dev/cu.YOUR_DEVICE
monitor_port = /dev/cu.YOUR_DEVICE
```

The ordered `extra_configs` list loads this optional file after the committed definitions, so overrides do not depend on filesystem enumeration order. Keep the same environment name and board/pin profile; new boards get new committed environments. There are no Wi-Fi credentials or services in this scaffold.

`make validate` is the broad check. Use `make test`, `make build-hosts`, or `make build-tag` while iterating. Build/test never uploads firmware. The CC2510 pre-build script rejects upload targets even if requested, because the generic serial uploader does not speak TI debug.

To run the same checks individually, activate the virtual environment with `source .venv/bin/activate` on macOS/Linux or `.venv\Scripts\Activate.ps1` in Windows PowerShell, then run these from the repository root:

```sh
python tools/etag.py check
python -m unittest discover -s tests -v
pio test -e native
pio run -e cardputer-adv -e esp32-devkit -e esp32-s3-devkit
pio run -d firmware/targets/cc2510
```

The root project creates firmware for the Cardputer/ESP32. The final command creates the separate tag diagnostic image. See [the application guide](../firmware/programmer/README.md) and [target projects](../firmware/targets/README.md) for their distinct roles.

A successful ESP32 compile checks toolchain/library compatibility, not keyboard, SD, voltage, reset timing, or tag communication. Record physical validation in the per-host and per-tag hardware notes. CI artifacts distinguish `programmer-*` binaries from `cc2510f32-smoke-compile-only` HEX files.

The [GitHub Actions workflow](../.github/workflows/ci.yml) first validates the catalog and runs Python/native tests, then builds all three ESP32 hosts and the independent CC2510 example. It never uploads to connected devices. Python is selected from `.python-version`; the pip cache explicitly hashes both `requirements-dev.txt` and `requirements-lock.txt`, since the default cache lookup expects a differently named requirements file. The actions use pinned release commits with the Node 24 runtime.

Artifact uploads explicitly include the selected files inside `.pio` and fail if no outputs are found. They contain ESP32 application binaries/debug ELFs or CC2510 diagnostic HEX/map files, rather than a complete installation bundle. Use PlatformIO's local upload command for the ESP32 bootloader and partition offsets. CI success establishes software/build compatibility; physical tag support is recorded separately.
