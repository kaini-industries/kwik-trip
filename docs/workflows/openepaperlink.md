# Display updates through OpenEPaperLink

The workbench can send artwork through an existing **OpenEPaperLink access point (AP)** to tags already running compatible firmware and registered on that AP. The Cardputer uses Wi-Fi; the AP supplies the tag radio. This route requires no CC2500 attached to the Cardputer. It updates display content and does not flash tag firmware.

The **SES-imagotag ERD3-0210-A and EDG3-0220-B/GL440 records in this project remain unverified for this route**. Neither an AP listing nor an upstream family resemblance establishes factory-firmware compatibility. Prepare a supported AP/tag setup using the [upstream documentation](https://github.com/OpenEPaperLink/OpenEPaperLink/wiki) before using these clients. The [pinned integration reference](../reference/openepaperlink.md) records the API and limits of the upstream evidence.

## From the Cardputer Advance

Use a Cardputer build that includes the **4 OpenEPaperLink** menu choice. The other ESP32 console builds do not provide this screen.

1. Copy [config/oepl.example.json](../../config/oepl.example.json) to a FAT32 microSD card as `/etag/oepl.local.json`. Set `ssid`, `password`, and the AP address while retaining `"schema_version": 1`. This mode accepts an HTTP origin with a numeric IPv4 address, for example `http://192.168.1.50` or `http://192.168.1.50:8080`. Hostnames, HTTPS, credentials in URLs, and URL paths are unsupported on the Cardputer.
2. Put baseline JPEG artwork in `/etag/images`. Each image must have the AP-reported tag width and height exactly, and be no larger than **512 KiB**. The browser editor's **Save JPEG for SD** action prepares this format; copy its downloaded file to the card. Other editors should export an ordinary 8-bit RGB or grayscale JPEG with progressive encoding disabled. The image library inspects at most 16 supported image entries and this mode shows only JPEGs among them; remove unrelated entries if a JPEG is absent.
3. Insert the card, choose **4 OpenEPaperLink**, and press **Enter** to connect. Wi-Fi does not start until you choose Connect. If you edited the SD configuration, press **R** on the setup screen to reload it.
4. Use **1–3** to select a tag. **N/P** moves through the currently loaded rows; **N** at the end fetches the AP's next batch when available. **P** does not fetch a prior remote batch; **R** starts the list again. The detail screen shows that tag's MAC, AP-reported dimensions, battery, RSSI, and pending count. **R** refreshes its status.
5. Press **Enter** for SD images, then **1–3** to prepare a preview. Review the destination MAC and artwork. **D** toggles AP dithering; **Enter** on this preview explicitly submits the image. The Cardputer rechecks tag registration, display dimensions, and the file's hash before submitting it.
6. Read the result, then check the physical display after the tag checks in. Backspace returns through the screens. Exiting from setup or the tag list disconnects and turns Wi-Fi off.

Configuration stays on the SD card; this mode does not save Wi-Fi credentials to NVS or to the project catalog. Keep the populated file private. The AP connection uses plain HTTP on your local network. For HTTPS or a hostname, use the desktop route below.

The Cardputer accepts dimensions up to 4096 on each axis with at most 8,388,608 pixels, but a successfully decoded local preview is also required. Its AP responses are limited to 16 KiB and each fetched tag batch to 32 records. Large batches or low memory produce an error; use Studio if needed. Rebooting after a BLE session can recover memory held by the Bluetooth controller.

During network work, **Backspace requests cancellation and exit**. The screen waits for the worker to stop before turning Wi-Fi off. Cancellation cannot retract data already sent or work the AP already scheduled. Connection attempts and reads have finite limits; an upload can run for up to its 60-second exchange limit before timing out. The progress percentage counts bytes read from the SD file for transmission, not tag delivery. A cancelled or interrupted upload may have an unknown outcome, so check the AP before sending again.

## From the browser editor

Run this from the project root:

```sh
make studio
```

Open [localhost:8000](http://localhost:8000), choose **OEPL RF**, enter the AP's explicit `http://` or `https://` address, and load its tags. The desktop bridge supports DNS names and standard HTTPS certificate verification. It must reach the AP from your computer; an internet connection alone does not provide access to a private LAN AP.

Select a tag, then prepare artwork by copying the current editor canvas, choosing an image, or entering text. Review the fitted preview and dithering choice, then use the explicit Send action. Preparing artwork, choosing a tag, and refreshing status do not upload anything. A fresh tag registration/dimension check occurs before submission.

For the Cardputer route, choose **Save JPEG for SD** instead. This downloads `etag-<MAC>.jpg` from the same preview, enforces the Cardputer's 512 KiB limit, and sends no image to the AP. Copy it to `/etag/images` on the SD card; select the destination and AP dithering setting on the Cardputer before sending.

The browser exports a baseline JPEG at the AP-reported dimensions. Desktop uploads are limited to **2 MiB** and **2048 × 2048 pixels**. The preview shows the submitted artwork; the AP applies its own color palette, dithering, rotation, and panel conversion, so the final display can differ. `bpp` alone does not identify the panel's colors.

Studio listens only on your computer's loopback interface and serves `web/`. Browser requests use a same-origin bridge with Host/Origin checks. AP addresses and tag lists are not saved by this integration. USB/BLE editing remains available in the existing editor; an OEPL submission does not pass through the Cardputer's IR workflow.

## From the command line

Replace the example AP address, MAC, and image path:

```sh
.venv/bin/python tools/oepl.py tags --ap http://192.168.1.50
.venv/bin/python tools/oepl.py status --ap http://192.168.1.50 --mac 00000123456789AB
.venv/bin/python tools/oepl.py upload artwork.jpg --ap http://192.168.1.50 --mac 00000123456789AB --dither 0
```

The first two commands only read AP state. The upload command explicitly changes the selected tag's AP-managed content. It uses the same registration, metadata, JPEG, and dimension checks as the browser bridge. MACs contain 12 or 16 hexadecimal digits without separators; 12-digit values gain four leading zeroes. Command output is JSON. See [tools/README.md](../../tools/README.md#openepaperlink-access-point-client) for the full contract and failure behavior.

Desktop requests use an 8-second socket timeout and check a 30-second operation deadline between requests and response-body reads. DNS resolution follows operating-system limits, and slow response headers can exceed that deadline. The client rejects redirects, malformed/oversized responses, and looping pagination. There is no automatic retry of image submissions.

## What the results establish

**AP accepted/submitted means the AP returned its expected upload response. It does not confirm a display refresh.** The API always reports `displayConfirmed: false`. An empty HTTP 200 is not accepted as success. A timeout or cancellation after transmission starts may leave the result unknown.

The AP processes an image upload by selecting its external-image content mode (`24`) for that tag. This can replace its previous scheduled content mode; another automation or AP action can later overwrite the image. The clients leave the AP's alias, rotation, inversion, and lookup-table settings unchanged.

Status values and dimensions come from the AP's database and `/tagtypes` metadata. They remain operational observations, separate from verified hardware profiles and per-unit bench evidence. A tag's pending count returning to zero can reflect completion or a timeout. Transfer progress, a successful HTTP response, and successful software tests are insufficient evidence that the intended physical display changed.

Before recording hardware support, perform the [Cardputer validation procedure](cardputer-validation.md) and record an OEPL-specific bench result: AP/tag firmware versions, tag model and PCB revision, source revision, destination MAC, input dimensions, displayed orientation/colors, and observed refresh. Start with a distinctive small pattern on one supported tag. Exercise a normal update, an unreachable AP, missing metadata, a wrong-sized image, and a cancelled upload; confirm the display directly and check the AP before repeating an ambiguous submission. No physical AP/tag validation was performed as part of this software integration.
