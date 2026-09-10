# Editing a tag display with Cardputer Advance

This workflow sends content to a compatible tag's existing infrared interface. It does not replace the tag firmware. The Cardputer itself runs the application built from the root PlatformIO project.

## Compatibility

The first display backend supports PP4/PP16 infrared protocols. Upstream reports working SmartTag HD S Red (152×152), HD M Red (208×112), and HD110 Red (400×300). Its other model presets are research data, not blanket validation. Radio-only labels do not become compatible by choosing a matching size. Our SES/Vusion research profiles have no established IR compatibility.

Keep normal tag battery power for infrared editing; there is no wired adapter involved. Investigate one known unit at a time. A 17-character barcode helps derive an address and a model hint; it is not a universal e-tag identifier or proof that this protocol is supported. Segment devices can be saved for reference but do not expose image operations in this UI.

## On the Cardputer

The main menu is **1 IR devices**, **2 Wired diagnostics**, **3 Browser editor**.

1. Choose IR devices → Add target. Enter a valid 17-character barcode including its checksum. Confirm the detected model settings. Unknown models require explicit kind, size, orientation and color settings.
2. Give it a name. The device stores up to nine records, and preserves names/settings across restart. A storage error is displayed; failed writes do not silently replace a saved record.
3. Select the device and choose Text, Image, Blink LED or Details. Details provides profile editing and removal. Removing a saved record does not erase the tag.
4. For Text, use the keyboard and the on-screen style controls. For Image, put PNG/JPEG/BMP/QOI files under `/etag/images` on a FAT32 SD card; the current browser lists up to 16 files. Keep filenames short enough for the 96-byte full-path limit.
5. Aim at the tag receiver and send. Backspace cancels; Enter can repeat completed artwork. Reliable mode repeats PP16 data packets four times; Fast uses three. Both retain the wake sequence. Prefer Reliable for the first hardware check.

**IR transmitted** means that the Cardputer emitted the frames. It has no receive path for tag acknowledgements. Wait for the display refresh and check it visually. A missing update does not establish a firmware problem.

## Browser editor

Run `make studio` at the project root, then open [localhost:8000](http://localhost:8000) on the same computer. This serves the included `web/` directly. The editor contains its scanner library locally; optional upstream template fetch buttons contact their named external services only when used.

On the Cardputer choose **3 → 1 USB serial**, then select USB SERIAL in the browser and connect. Use a data-capable USB cable and close PlatformIO's serial monitor first. Desktop Chrome or Edge is a useful starting point; browser support varies by operating system. The editor's normal image workflow also works without camera access.

For Bluetooth, choose **3 → 2 Bluetooth** on the Cardputer and BLUETOOTH in the editor. Look for `etag Cardputer XXXX`. The Cardputer shows a new six-digit pairing code when Bluetooth mode starts; enter it when the browser or operating system prompts. Commands and notifications require authenticated, encrypted BLE. Only that selected transport accepts commands for the session. Exit the Cardputer browser screen to stop advertising and disconnect. The BLE service object/controller are retained for reuse until reboot; reboot and select USB if image preparation runs low on memory.

Add a device using its barcode. Manual entry provides a display preset for an unknown model; raw hexadecimal IDs are not supported. Custom dimensions, palette, and orientation can be edited on the Cardputer. The current profiles select PP16 for graphic displays and PP4 for segment displays; there is no manual modulation override in this UI. Device saves await an acknowledgement from the Cardputer.

Select a device, prepare/dither the artwork, and push it to the Cardputer. Upload chunks have offsets and acknowledgements; encoded image sizes and compression are validated. Uploading stages artwork without transmitting it. The browser records the acknowledged target, page, content revision and Cardputer stage token; Send is refused if any of them changes. Aim at the tag and press Enter on the Cardputer or the browser's Send control. Cancel/back clears or exits as indicated on-screen.

## Relationship to programming

IR device addresses/display settings live in Cardputer NVS. They are distinct from the evidence-backed hardware revision profiles in `profiles/tags/` and unit inventory in `hardware/inventory/`. Neither a barcode nor an IR update establishes MCU, voltage, flash capacity, or a programming pin map.

Choose **2 Wired diagnostics** from the idle main menu to use the existing `profiles`, `select`, `probe confirmed`, `sd`, and `save` commands. The chosen wired profile is also remembered. Type `display` to return. The browser API has no firmware read, erase, write, or wired-probe commands.
