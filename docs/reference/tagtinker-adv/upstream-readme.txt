# TagTinker ADV

<p align="center">
  <strong>Infrared Electronic Shelf Labels for M5Stack Cardputer ADV</strong><br>
  <sub>Custom images • Text • Digital art</sub>
</p>

<p align="center">
  <a href="LICENSE"><img alt="License: GPL-3.0-only" src="https://img.shields.io/badge/License-GPL--3.0--only-blue.svg"></a>
  <img alt="Platform: Cardputer ADV" src="https://img.shields.io/badge/Platform-Cardputer%20ADV-black.svg">
  <a href="https://i12bp8.github.io/tagtinker-adv/"><img alt="Web Studio" src="https://img.shields.io/badge/Web%20Studio-Open%20in%20browser-a78bfa?logo=github"></a>
</p>

<p align="center">
  <strong><a href="https://i12bp8.github.io/tagtinker-adv/">→ Launch TagTinker ADV Web Studio ←</a></strong>
</p>

<img alt="Demo Image"  src="/preview.jpg">

## Overview

TagTinker ADV turns the Cardputer ADV into a handheld infrared shelf-label editor. Type on the built-in keyboard, choose an image from microSD, or prepare artwork in the browser and transfer it over Bluetooth or USB.

The Cardputer sends the artwork directly to the tag through its built-in infrared emitter. No external transmitter or Wi-Fi board is needed.

## Features

- **On-device text editor:** Choose fonts, size, alignment, foreground, and background colors. Text scales to fit the selected tag.
- **Web Studio:** Import images, adjust contrast and threshold, dither, invert, and preview the result before sending. Connect over Web Bluetooth or Web Serial.
- **Barcode scanning:** Use the browser's camera scanner or enter the tag's printed barcode to identify its address and display profile.
- **Saved targets:** Store up to nine tags on the Cardputer, assign names, and override display profiles when needed.
- **microSD images:** Load PNG, JPEG, BMP, or QOI files from `/tagtinker/images` on a FAT32 card.
- **Color support:** Monochrome, red, yellow, and four-color graphic profiles, with separate image planes where required.
- **Browser templates:** Render GitHub statistics, cryptocurrency prices, or a clock as tag artwork.
- **Tag controls:** Send images and text, blink a graphic tag's LED, switch pages, or display a diagnostic screen.
- **Reliable and Fast modes:** Choose between the default transmission settings and fewer data-packet repeats.

## Supported tags

https://www.furrtek.org/?a=esl read this documentation from Furrtek to get to understand which tags are vulnerable and why.

TagTinker ADV supports infrared electronic shelf labels with compatible PP4 or PP16 receivers. The following red-accent models have been used successfully for text and images:

| Model | Resolution | Colors |
| --- | --- | --- |
| SmartTag HD S Red | 152 × 152 | Black, white, red |
| SmartTag HD M Red | 208 × 112 | Black, white, red |
| SmartTag HD110 Red | 400 × 300 | Black, white, red |

Additional profiles are listed in [TargetProtocol.cpp](src/TargetProtocol.cpp). Unknown models require a manual profile. Segment tags support page commands rather than image uploads. Radio-only labels are not supported.

## Getting started

1. Build and flash the firmware using the commands below.
2. On the Cardputer, open **Target** with `2` and add your tag using its printed 17-character barcode.
3. Select the tag and choose **Text** or **Image**. For images from microSD, place files in `/tagtinker/images` first.
4. Aim the Cardputer's infrared emitter directly at the tag's receiver and press `Enter` to send.
5. Keep the device steady until transmission finishes, then allow several seconds for the e-paper display to refresh.

### Using Web Studio

1. Press `3` on the Cardputer's main menu to open **WebUI**.
2. Open **[TagTinker ADV Web Studio](https://i12bp8.github.io/tagtinker-adv/)** in a browser with Web Bluetooth or Web Serial support.
3. Connect to the Cardputer over Bluetooth, or use a USB data cable with Web Serial.
4. Select a saved tag, import an image or render a template, and adjust the preview.
5. Upload the artwork to the Cardputer. Once ready, aim at the tag and press `Enter` on the device or use the browser's send button.

Artwork stays staged on the Cardputer so you can repeat a send without uploading it again.

## Controls

| Key | Action |
| --- | --- |
| `1` on the main menu | Broadcast controls |
| `2` on the main menu | Saved targets and text/image editing |
| `3` on the main menu | Web Studio connection |
| `Enter` | Confirm, send, or repeat |
| `Backspace` | Delete while editing; cancel or go back elsewhere |
| `Tab` on a graphic tag's action menu | Switch Reliable / Fast |
| `Tab` on the staged WebUI screen | Switch Reliable / Fast |

### Transmission modes

**Reliable** is selected after boot and sends four copies of each PP16 data packet.

**Fast** sends three copies, reducing the data phase by about 25%. The 4.2-second wake sequence, carrier timing, packet gaps, and refresh sequence remain the same, so the total saving depends on image size and compression. Small text updates benefit less than large images.

Change the mode on the Cardputer or with the browser's **Transmission Mode** selector. The choice lasts until reboot. If a tag misses an update in Fast mode, switch back to Reliable. PP4 transfers use the same settings in both modes.

## Build and flash

Requires PlatformIO and a USB-C data cable.

```sh
pio run
pio run --target upload
```

The build produces `.pio/build/m5stack-cardputer/firmware.bin`.

## FAQ

**Do I need extra hardware?**

The Cardputer ADV's built-in infrared emitter handles transmission. A microSD card is optional; text editing and browser uploads work without one.

**Does the tag need Bluetooth or Wi-Fi?**

No. Bluetooth or USB connects the browser to the Cardputer. The Cardputer communicates with the tag over infrared.

**Does “Transmitted” confirm that the tag updated?**

It confirms that the Cardputer emitted the frames. The built-in transmitter does not receive tag acknowledgements. Check the display after its refresh, and repeat if needed.

**Why do some images take longer?**

Text and simple graphics compress well. Detailed or heavily dithered images require more data packets. Both transmission modes also include the wake sequence needed to reach sleeping tags.

**Can I use the original Cardputer?**

This project targets the Cardputer ADV. Other hardware has not been validated.

## Development

Run the regression tests with a C++17 compiler and Node.js:

```sh
bash scripts/test.sh
```

The C++ tests use address and undefined-behavior sanitizers. Tests cover protocol frames, waveform timing, compression, and browser upload acknowledgements.

Edit the browser application in `web/`, then update the GitHub Pages assets:

```sh
python3 scripts/build_web.py
python3 scripts/build_web.py --check
```

| Directory | Contents |
| --- | --- |
| `src/` | Firmware, rendering, and infrared transmission |
| `web/` | Browser editor source and bundled scanner library |
| `docs/` | GitHub Pages assets and [protocol notes](docs/protocol.md) |
| `tests/` | Regression tests |
| `scripts/` | Test runner and web asset synchronization |

## Responsible use

Use TagTinker ADV with tags you own or have permission to operate. Do not modify retail displays or other people's equipment without authorization.

## Credits for research

https://github.com/furrtek/PrecIR is the base that makes this whole project possible!

## License

Licensed under the **GNU General Public License v3.0 only** (GPL-3.0-only). See [LICENSE](LICENSE).

The bundled barcode library is distributed under its own [license](web/vendor/ZXING-LICENSE), which is also included with the GitHub Pages assets.
