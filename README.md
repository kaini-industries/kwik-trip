# etag

A portable e-tag workbench built around the **M5Stack Cardputer Advance**, with support for other ESP32 development boards. The goal is to investigate, back up, and reprogram a collection of electronic tags using shared tools and a profile for each hardware revision.

**This project already builds firmware for the Cardputer Advance.** It includes a **PP4/PP16 infrared display editor** with saved devices, text composition, SD images, and a browser editor over USB or Bluetooth. A separate diagnostics mode provides a **wired TI CC2510 identification/status probe** and SD logs. Reading or reprogramming tag firmware remains upcoming work.

The project is organized to accommodate different manufacturers, MCU families, and displays. Compatibility is established per hardware revision and operation. The included tag records are initial research examples. No tag model has been physically validated with this firmware yet.

## What runs where

| Component | Runs on | Purpose |
| --- | --- | --- |
| **Cardputer/ESP32 firmware** | Your Cardputer Advance or another configured ESP32 | Provides the user interface, sends display content over compatible infrared protocols, and probes CC2510 tags through a wired adapter |
| **Development tools** | Your computer | Build firmware, validate profiles, prepare image packages, and archive independently acquired backups |
| **Tag firmware** | The electronic tag's own MCU | Controls its display, peripherals, and power management; built separately for that hardware |

The root [PlatformIO project](platformio.ini) builds the Cardputer application from [firmware/programmer](firmware/programmer/README.md). Once installed, you can operate it using the Cardputer's keyboard and screen without a computer. Infrared editing uses the built-in emitter and the tag's normal battery supply. Wired diagnostics require a matching adapter and independently verified target supply.

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
| IR display updates, text editor, SD image library, saved devices | Implemented for compatible PP4/PP16 graphic tags; physical validation pending |
| Browser editor over USB serial or BLE | Implemented; codec/upload tests pass; physical connection validation pending |
| Custom tag display drivers | Separate target firmware work; not implemented |

Adding a profile records a device; it does not implement its protocol. See the [roadmap](docs/plans/roadmap.md) for the path from diagnostics to a portable programmer.

## Start with the Cardputer Advance

**Prebuilt firmware is committed in [firmware/releases/cardputer-adv](firmware/releases/cardputer-adv/README.md).** You can install it without compiling:

| Installer | Download |
| --- | --- |
| **M5Burner 3** or **standard ESP32 USB flashing** | [cardputer-adv-factory.bin](firmware/releases/cardputer-adv/cardputer-adv-factory.bin), complete image at **0x0** |
| **bmorcelli/Launcher**, through its SD browser or WebUI | [cardputer-adv-app.bin](firmware/releases/cardputer-adv/cardputer-adv-app.bin), application only; Launcher chooses its partition |

On GitHub, open the file and select **Download raw file**. These images are for the **Cardputer Advance**, not the electronic tags or the original Cardputer. Build and image-format checks pass; physical validation is pending.

**Choose one installation method below.** Factory USB installation replaces an existing Launcher setup and clears saved Cardputer settings, including saved etag devices. To keep Launcher, install the application through Launcher itself. Keep tag wiring disconnected while installing host firmware.

### Flash with Launcher

1. Start with [bmorcelli/Launcher](https://github.com/bmorcelli/Launcher) installed on your Cardputer Advance, using a release that supports the Advance keyboard.
2. Download **cardputer-adv-app.bin** from the table above and copy it to a FAT32 microSD card, for example `/firmware/cardputer-adv-app.bin`.
3. Insert the card, restart the Cardputer and press **Enter** at Launcher's startup screen to open its menu.
4. Open **SD**, select the `.bin` and choose **Install**. Let Launcher select/create the destination partition, then boot the installed application.

Alternatively, open Launcher's **WUI**, visit the address it displays, and use its firmware/OTA upload to select **cardputer-adv-app.bin**. No separate filesystem image is needed. Return to Launcher using its startup selection screen.

Do not write the app directly to `0x10000` with a USB flashing tool on a Launcher installation; Launcher manages its own partition layout. For this private GitHub repository, download while signed in, then use SD or WebUI rather than an anonymous online download link.

### Flash with M5Burner 3

M5Burner 3 uses an account-based custom-firmware upload workflow. The factory image is ready for that workflow; it is not already published in M5Burner's catalog.

1. Download **cardputer-adv-factory.bin** from the table above. Connect the Cardputer Advance with a USB data cable and close any serial monitor.
2. Sign into M5Burner with your M5Stack community account, then open **USER CUSTOM → Publish**.
3. Select the factory file under **FirmWare**. Fill in the name, version, description, device type, GitHub URL and cover as prompted, then click **Upload**. Use a name such as **etag — Cardputer Advance**, select **Cardputer ADV** when listed, and use the `source_commit` from [manifest.json](firmware/releases/cardputer-adv/manifest.json) as the version identifier.
4. Use the uploaded entry's **Share** action to obtain a **Share Code**, then open **Share Burn**. Select the Cardputer's USB port, choose baud **460800** and start the burn. Restart the Cardputer when it finishes.

M5Burner writes this complete image at **0x0**. Do not select the app-only image for this route. [M5Stack's publishing guide](https://docs.m5stack.com/en/uiflow/m5burner/publish) explains the upload, publicity and sharing controls. To flash locally without uploading to M5Stack's service, use esptool below.

### Flash over USB with esptool

Download **cardputer-adv-factory.bin** into a folder and open a terminal in that folder. With Python **3.12** installed, set up the pinned flashing tool and list available serial ports.

**macOS / Linux:**

```sh
python3.12 -m venv .venv
.venv/bin/python -m pip install esptool==4.9.0
.venv/bin/python -m serial.tools.list_ports
```

Connect with a USB data cable, close any serial monitor, and replace `/dev/cu.YOUR_DEVICE` with the port from that list. Linux ports commonly look like `/dev/ttyACM0`.

```sh
.venv/bin/python -m esptool --chip esp32s3 --port /dev/cu.YOUR_DEVICE --baud 460800 write_flash 0x0 cardputer-adv-factory.bin
```

**Windows PowerShell:**

```powershell
py -3.12 -m venv .venv
.\.venv\Scripts\python.exe -m pip install esptool==4.9.0
.\.venv\Scripts\python.exe -m serial.tools.list_ports
# Replace COM5 with the Cardputer's port from the list.
.\.venv\Scripts\python.exe -m esptool --chip esp32s3 --port COM5 --baud 460800 write_flash 0x0 cardputer-adv-factory.bin
```

Wait for successful write verification, then restart the Cardputer. The factory image includes the bootloader, partition table, initial OTA data and application; no additional `.bin` files or separate full-chip erase are needed. Other ESP32 flash tools should use **ESP32-S3**, address **0x0**, **8 MB flash**, and preserve the image's **DIO / 80 MHz** settings. The commands above use esptool 4.9.0; esptool 5 calls the operation `write-flash`.

If the port is missing or the connection fails, check the data cable and close programs using that port. To enter download mode manually, hold the Stamp's **G0** button while connecting USB, release it, then retry. Lower the baud to **115200** if transfers fail. See [M5Stack's Cardputer Advance guide](https://docs.m5stack.com/en/core/Cardputer-Adv) for download mode.

### Verify downloads and update an existing installation

Download both binaries, [manifest.json](firmware/releases/cardputer-adv/manifest.json) and [SHA256SUMS](firmware/releases/cardputer-adv/SHA256SUMS) into the same folder. Before flashing, run `shasum -a 256 -c SHA256SUMS` on macOS or `sha256sum -c SHA256SUMS` on Linux. On Windows, compare `Get-FileHash .\cardputer-adv-factory.bin -Algorithm SHA256` with its entry in `SHA256SUMS` (substitute the app filename for a Launcher install).

For an existing **standalone etag installation with the matching partition layout and booting app0**, you can preserve saved settings by flashing **cardputer-adv-app.bin at 0x10000**. This is not the Launcher procedure. Follow the [detailed installation guide](firmware/releases/cardputer-adv/README.md#standard-esp32-usb-flashing) for that update command and the full flash layout.

After any installation method, the Cardputer should open the workbench menu: **1 IR devices**, **2 Wired diagnostics**, and **3 Browser editor**. Continue with [display editing](#edit-a-compatible-tag-display) or [wired diagnostics](#connect-a-tag-for-wired-diagnostics).

### Build from source

Open [etag.code-workspace](etag.code-workspace) in VS Code, or use a terminal in the project root. With Python 3.12 installed:

```sh
python3.12 -m venv .venv
.venv/bin/python -m pip install -r requirements-dev.txt
.venv/bin/pio run -e cardputer-adv
```

The application output is `.pio/build/cardputer-adv/firmware.bin`. `make package-cardputer` also generates the complete factory image and named application image under `.pio/build/cardputer-adv/release/`. PlatformIO's upload command installs the separate build components at their correct offsets.

Keep the tag disconnected while installing firmware on the **Cardputer**, and replace `/dev/cu.YOUR_DEVICE` with its actual USB port:

```sh
.venv/bin/pio device list
.venv/bin/pio run -e cardputer-adv -t upload --upload-port /dev/cu.YOUR_DEVICE
```

After boot, the Cardputer menu offers **1 IR devices**, **2 Wired diagnostics**, and **3 Browser editor**. Choose **2**, then type `help` for the keyboard or USB diagnostics console. Type `display` to return to the menu. For the optional computer console:

```sh
.venv/bin/pio device monitor --port /dev/cu.YOUR_DEVICE --baud 115200
```

On Windows, create the environment with Python 3.12, activate `.venv\Scripts\Activate.ps1`, and use `python` / `pio` instead of `.venv/bin/` commands. The [build guide](docs/development.md) covers pinned versions and configuring the PlatformIO IDE to use this environment.

## Edit a compatible tag display

This mode updates content through the tag's existing firmware. It does not install custom firmware. It needs a compatible **PP4/PP16 infrared receiver**; a matching screen size or manufacturer alone does not establish compatibility. Our initial SES/Vusion research profiles have not been verified for IR. Upstream reports success on SmartTag HD S Red, HD M Red and HD110 Red; our integrated build still needs bench validation.

1. Choose **1 IR devices**, then **1 Add target**. Enter the tag's 17-character barcode. Name it and check its inferred display settings; unknown models require an explicit profile.
2. Select a saved device, then **Text** to compose with the keyboard, or **Image** to select a PNG, JPEG, BMP or QOI from `/etag/images` on a FAT32 microSD card.
3. Aim the built-in emitter at that tag and press Enter when ready. Backspace cancels a transmission. **IR transmitted** confirms emission; visually check the tag after refresh because the Cardputer cannot receive acknowledgements.

Up to nine IR devices are saved on the Cardputer. These records describe addresses and display settings; they do not mark hardware verified or authorize firmware programming. Segment records can be cataloged, but this UI provides image/text/blink operations only for graphic profiles.

For the browser editor, run `make studio` on your computer and open [localhost:8000](http://localhost:8000). On the Cardputer choose **3 Browser editor → 1 USB serial** or **2 Bluetooth**, then select the same connection in the browser. USB is the recommended starting point. Prepare and preview artwork, push it to the Cardputer, then explicitly send it to the tag. Close the serial monitor before connecting the browser over USB.

See the [display editing guide](docs/workflows/display-editing.md), [hardware test procedure](docs/workflows/cardputer-validation.md), and [TagTinker attribution](THIRD_PARTY_NOTICES.md).

## Connect a tag for wired diagnostics

First follow [adding a tag to your collection](docs/workflows/add-a-tag.md). Identify its MCU, hardware revision, voltage, and debug pads before choosing an interface. The [Cardputer adapter guide](hardware/hosts/cardputer-adv/README.md) describes the current wired CC2510 adapter; other interfaces need their own wiring and implementation.

| Command | What it does |
| --- | --- |
| `help`, `about`, `pins` | Show commands, host details, and configured debug GPIOs |
| `profiles` | List compiled-in tag profiles and the current selection |
| `select <profile-id>` | Select the hardware profile for the connected tag |
| `probe confirmed` | Run the CC2510 identification/status probe after wiring and power checks |
| `display` | Return from the wired console to the Cardputer menu |
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
| `cardputer-adv` (default) | Cardputer Advance / Stamp-S3A, 8 MB, no PSRAM | Keyboard, screen, IR editor, USB/BLE browser connection, SD images/logs |
| `esp32-devkit` | Classic ESP32-WROOM DevKit, 4 MB | Serial console |
| `esp32-s3-devkit` | ESP32-S3-DevKitC-1 N8, no PSRAM | Native USB console |
| `native` | Development computer | C++ tests using a simulated debug wire |

Build another host with `.venv/bin/pio run -e esp32-devkit` or `.venv/bin/pio run -e esp32-s3-devkit`. Additional boards and the original Cardputer need their own configuration and verified pin assignments.

Run the complete software checks and firmware build matrix with:

```sh
make validate
```

This validates profiles, reference hashes and documentation links, runs Python/C++/browser tests, and builds all three ESP32 environments plus the CC2510 example. Display tests require a C++17 compiler with AddressSanitizer/UBSan and Node.js 22 or newer. It does not upload to hardware. The Makefile and supplied editor tasks use macOS/Linux virtual-environment paths; individual commands are in the [build guide](docs/development.md).

## Project guide

| Location | Contents |
| --- | --- |
| [firmware/programmer](firmware/programmer/README.md), [lib/EtagCore](lib/EtagCore) | Cardputer/ESP32 application and portable protocol code |
| [firmware/targets](firmware/targets/README.md) | Firmware projects for the tags themselves |
| [firmware/releases/cardputer-adv](firmware/releases/cardputer-adv/README.md) | Committed Cardputer factory/app downloads, checksums and installation guide |
| [config](config), [profiles](profiles/README.md) | Host definitions, pinned build environments, and tag catalog |
| [hardware](hardware/README.md) | Tag photos, adapter documentation, pad maps, and inventory templates |
| [tools](tools/README.md) | Profile, image package, verification, and backup tools |
| [docs](docs/README.md) | Workflows, architecture, development roadmap, and research references |
| [web](web) | Local browser editor; source includes its own third-party notices |
| [test](test), [tests](tests) | C++, Python and browser tests |
| [backups](backups/README.md), [artifacts](artifacts/README.md) | Local acquisitions and generated outputs, ignored by Git |

The original model-specific research and photos remain in the [reference archive](docs/reference/README.md). Their original-file hashes are checked against the [migration manifest](docs/reference/migration-manifest.json).

## License

Project source is licensed under [GPL-3.0-only](LICENSE). The display editor incorporates TagTinker ADV at a pinned revision; see [third-party notices](THIRD_PARTY_NOTICES.md) for provenance, modifications, and exclusions for archived reference material and dependency assets.
