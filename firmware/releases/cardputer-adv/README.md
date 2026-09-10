# Cardputer Advance firmware downloads

These are real binaries committed to Git, built for **M5Stack Cardputer Advance (ESP32-S3, 8 MB flash, no PSRAM)**. Open a `.bin` on GitHub and select **Download raw file**. Access to this private repository is required.

| File | Use |
| --- | --- |
| [cardputer-adv-factory.bin](cardputer-adv-factory.bin) | M5Burner 3 firmware upload; standard ESP32 USB installation at **0x0** |
| [cardputer-adv-app.bin](cardputer-adv-app.bin) | **Launcher SD/WebUI installation**; application-only update of a matching standalone etag installation at **0x10000** |
| [SHA256SUMS](SHA256SUMS) | SHA-256 checksums for both images and the manifest |
| [manifest.json](manifest.json) | Source revision, build-input hashes, tools, image sizes and flash layout |

Both images contain the same etag application. They install software on the **Cardputer**, not on an electronic tag. The workbench supports compatible IR display editing and wired CC2510 diagnostics; tag flash programming is still planned. See the [main README](../../../README.md) for capabilities and the [bench procedure](../../../docs/workflows/cardputer-validation.md) for physical validation.

## Launcher: keep your launcher

1. Install [bmorcelli/Launcher](https://github.com/bmorcelli/Launcher) for **Cardputer/Cardputer ADV**, using a release that supports the Advance keyboard.
2. Copy **cardputer-adv-app.bin** onto your FAT32 microSD card (for example `/firmware/cardputer-adv-app.bin`).
3. Open Launcher's **SD** browser, select that file and choose **Install**. Its **WUI** firmware upload is another option.
4. Let Launcher choose/create the application partition, then boot the installed app. Return to Launcher using its startup selection screen.

Do not use a raw USB write to `0x10000` on a Launcher installation: Launcher owns the partition layout and chooses the destination. The application uses the active partition table and NVS APIs, with no hard-coded flash partition addresses or required SPIFFS image. SD content stays on the microSD card. It needs enough free flash for the application, rounded up to the partition alignment shown by Launcher.

Launcher also recognizes the factory image's embedded partition table, but the **app** image is preferred here because this application requires no bundled filesystem. Flashing the factory image over USB replaces Launcher's bootloader/partition setup with the standalone etag layout.

Private GitHub download links cannot be fetched anonymously by Launcher's online catalog. Download while signed into GitHub, then use SD or WebUI. No GitHub token belongs in a firmware file or public catalog entry.

## M5Burner 3

Use **cardputer-adv-factory.bin**. M5Burner 3 burns its firmware payload at `0x0`; the bootloader, partition table, initial OTA selection data and application are all included in this file. The app-only image is unsuitable for M5Burner's zero-offset burn.

M5Burner 3's documented custom-firmware workflow is account-based: sign in, open **USER CUSTOM → Publish**, select the factory file under **FirmWare**, provide the name/version/device/description, then **Upload**. Select **Cardputer ADV** when available, and make the Advance-only requirement explicit in the description. Use the manifest's source revision as the version identifier. M5Burner can then burn the uploaded entry or provide a **Share Code** for a friend. See [M5Stack's publishing guide](https://docs.m5stack.com/en/uiflow/m5burner/publish).

The factory file is prepared for that workflow; this repository does not create an M5Burner account, upload to its service, or publish a catalog listing. An old M5Burner 2 ZIP/import manifest is not the M5Burner 3 upload format. For a local installation without a service upload, use standard ESP32 flashing below or Launcher.

Select the Cardputer USB port and baud **460800** (reduce to **115200** if needed). A factory burn clears saved etag devices/profile selection and other settings in the image's NVS range. Keep tag wiring disconnected during host installation.

## Standard ESP32 USB flashing

Install the project's [development environment](../../../docs/development.md), or install `esptool==4.9.0` into a Python virtual environment. With both downloads in your current folder, verify them first:

```sh
# macOS
shasum -a 256 -c SHA256SUMS
# Linux: sha256sum -c SHA256SUMS
```

Download `manifest.json` too when using the checksum command. On Windows, compare `Get-FileHash .\cardputer-adv-factory.bin -Algorithm SHA256` with `SHA256SUMS`.

Use a USB data cable and close any serial monitor. Replace the port below with the Cardputer's port (`COM5` is a Windows example):

```sh
python -m esptool --chip esp32s3 --port /dev/cu.YOUR_DEVICE --baud 460800 write_flash 0x0 cardputer-adv-factory.bin
```

The command is for pinned esptool **4.9.0**. Esptool 5 spells the command `write-flash`. For other flash tools, select **ESP32-S3**, **8 MB**, image address **0x0**, and preserve the file's flash settings (**DIO, 80 MHz**). The Arduino bootloader initializes the board's QIO flash operation itself. This is an ordinary unencrypted ESP32-S3 image.

If automatic download mode fails, follow [M5Stack's Cardputer Advance download-mode procedure](https://docs.m5stack.com/en/core/Cardputer-Adv): hold the Stamp's **G0** button while connecting USB, then release and retry. Restart after installation.

The factory file writes the complete range from `0x0` through the application, including erased padding. This resets NVS at `0x9000–0xDFFF`, including saved etag targets. It does not overwrite every byte of the 8 MB flash or the microSD card. A separate full-chip erase is not required for this installation.

For an existing **standalone etag installation using this exact partition layout and booting app0**, an app-only update preserves NVS:

```sh
python -m esptool --chip esp32s3 --port /dev/cu.YOUR_DEVICE --baud 460800 write_flash 0x10000 cardputer-adv-app.bin
```

## Layout and verification

| Factory file/flash offset | Payload |
| --- | --- |
| `0x0` | ESP32-S3 bootloader |
| `0x8000` | Arduino default 8 MB partition table |
| `0xE000` | Initial OTA selection data (`boot_app0.bin`) |
| `0x10000` | etag application, identical to `cardputer-adv-app.bin` |

The packager reads PlatformIO's actual upload images and flash settings, merges with pinned esptool, and rejects an unexpected layout. CI checks file/segment hashes, ESP32-S3 image checksums and digests, the partition table MD5, erased gaps, app capacity, and matching build-source fingerprints. It builds fresh factory/app images as an additional `cardputer-adv-flashable` artifact. Cross-platform builds may embed different timestamps; CI checks source freshness rather than requiring byte-identical builds.

Format compatibility was reviewed against [Launcher's SD installer at commit 55f8ea5](https://github.com/bmorcelli/Launcher/blob/55f8ea5edb284a6a53c4240fbb25aec30680ce5c/src/sd_functions.cpp), installed M5Burner 3.0.0's zero-offset burn implementation, and [Espressif's merged-image specification](https://docs.espressif.com/projects/esptool/en/release-v4/esp32/esptool/basic-commands.html#merge-binaries-for-flashing-merge-bin). Physical installation, keyboard, IR and tag communication still require a Cardputer Advance bench test; build/format checks do not establish that validation.

To regenerate these committed downloads, follow [the release workflow](../../../docs/development.md#refresh-the-committed-cardputer-binaries). Corresponding application source is in this repository at the manifest's `source_commit`, with exact inputs listed in `source_inputs`. Build dependencies are pinned in the configuration. See [GPL-3.0-only](../../../LICENSE) and [third-party notices](../../../THIRD_PARTY_NOTICES.md).
