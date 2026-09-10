# Architecture

The project is an e-tag workbench with the Cardputer Advance as its preferred portable host. Host hardware, tag records, protocol implementations, and tag applications are separate so support can grow across manufacturers and MCU families.

There are two independent firmware products. The root PlatformIO project builds the Cardputer/ESP32 application using Arduino C++; target projects build firmware using the compiler and memory model appropriate to each tag MCU. A `.bin` from the ESP32 project is never a tag firmware image. Computer-side tools handle validation, packaging, and backup archival.

```mermaid
flowchart TD
    H[Host JSON] --> G[PlatformIO pre-build validation]
    P[Tag JSON and evidence] --> G
    G --> C[Generated catalog per environment]
    C --> A[Console and Cardputer application]
    A --> B[Portable protocol logic: CC2510 probe today]
    B --> W[Arduino GPIO adapter]
    W --> T[Tag debug interface]
    B --> F[Fake wire in native tests]
    S[Separate tag firmware: SDCC example today] --> X[HEX]
    X --> K[Host package tool]
    P --> K
```

`tools/etaglib.py` validates profiles and host pins without third-party Python dependencies. `tools/pio_prepare.py` runs it before every embedded build, checks the PlatformIO board against the host profile, and creates `.pio/build/<environment>/generated/etag_catalog.h`. Hardware revision profiles remain compiled in. The browser connection parses bounded JSON commands on-device; it does not import programming profiles.

The current dispatcher constructs one `CcDebugProbe`, and the host pin schema describes TI DD/DC/RESET. The structure allows additional protocols, but runtime protocol registration, SWD/UART/SPI adapters, and their pin schemas are not implemented. New protocol metadata alone cannot create support. Extend dispatch, transport interfaces, host configuration and capability reporting together when adding a backend.

`EtagCore` owns protocol decisions, not GPIO calls. `CcDebugProbe` sends only GET_CHIP_ID and READ_STATUS. It reads identity twice, rejects missing/inconsistent/wrong-chip responses, and releases the target for every active probe outcome. Unsupported profiles return before entering debug mode. GPIO entry and each byte transfer are finite operations; there is no unbounded wait for a missing target.

The Arduino adapter owns data direction and reset. RESET is open-drain and relies on the target pull-up; DD is a bidirectional push-pull debug signal, not I2C. The operator supplies and verifies target power. The idle state disables all three drivers and pull resistors. The single main-loop command dispatcher owns the wire; the wired console does not run alongside IR transmission. The display mode owns a bounded RMT transmit task and enables the browser connection only when explicitly opened. A separate OpenEPaperLink mode uses Wi-Fi as an HTTP client; the Cardputer has no Wi-Fi server.

In wired diagnostics mode, Cardputer commands and serial commands use the same dispatcher. Serial and keyboard input have separate buffers; overlong lines are discarded. The screen wraps recent output, while serial carries full text. SD is mounted only on request and never auto-formatted. Probe records are created with unused filenames and remain distinct from binary backups.

Tag profiles describe shared hardware revisions; inventory records describe individual units. The initial research catalog contains one CC2510 hypothesis and one unknown model, and is intended to grow with the user's collection. Profile confidence, implemented operations, and successful bench validation are separate facts. Runtime probing can investigate a candidate only after the operator invokes `probe confirmed`. Neither a successful probe nor a package hash upgrades a profile to verified.

Package creation requires a verified profile and embeds a snapshot of it. The current format represents main flash starting at zero, filled to the recorded capacity with `0xFF` padding. Other memory layouts need explicit format/validation extensions. Verification also checks local evidence paths, so packages are currently workspace artifacts, not standalone exchange bundles or cryptographically signed firmware. The Cardputer does not yet consume these packages.

The `backups/` area holds per-unit acquisitions; `artifacts/` holds reproducible derived output. Both are ignored by Git. Archive originals and evidence under `hardware/` and `docs/reference/`, where checks protect the original-file hashes.

Curated Cardputer distribution binaries are committed separately under `firmware/releases/cardputer-adv/`. The PlatformIO `package` target merges its actual bootloader, partition table, initial OTA data and application into a zero-offset factory image for M5Burner/USB flashing. A separate application image lets Launcher select its own app partition. Checksums, image/layout validation and source fingerprints protect the downloads against accidental corruption and staleness.

## Display editing

TagTinker-derived protocol/frame/codec/waveform code lives under `lib/EtagCore/src/etag/display/` and remains independent of Arduino. The Cardputer adapters and menu live under `firmware/programmer/src/display/`; other ESP32 builds exclude those adapters. The existing TI core and tag-firmware projects remain independent.

`main.cpp` initializes M5Cardputer once and checks `board_M5CardputerADV` before initializing IR, SD, or either active workflow. The display menu is the default. Its wired-console handoff is available only from the idle main menu. In console mode, `display` switches ownership back. No browser command maps to wired probing, erase, or programming.

IR device records in NVS namespace `etag-ir` are per-unit address/display settings, distinct from the revision evidence catalog in `profiles/tags/`. They use a versioned blob containing names and settings together. Failed writes roll back the in-memory edit and return an error. Validated dimensions, address/barcode consistency, and operation capabilities are shared by persistence and the browser adapter. Neither barcode recognition nor successful emission upgrades hardware confidence. The wired profile selection is remembered separately in `etag-console`.

The transmitter uses GPIO44 from generated host metadata, a 1.25 MHz carrier and ESP32-S3 RMT TX channel 0 with four memory blocks. It owns the full frame waveform, repeats/wake timing, progress and cancellation. An image buffer remains owned by its caller until the asynchronous task finishes. Success means emitted, never acknowledged. No other IR library or peripheral may claim these RMT blocks.

USB and BLE browser sessions are mutually exclusive. USB does not initialize BLE. The BLE server is allocated lazily once and reused across sessions; exiting stops advertising and disconnects clients, while controller memory remains allocated until reboot. Reboot and choose USB for maximum available image memory. BLE requires secure-connections encryption with a displayed six-digit passkey; its command and notification attributes require authenticated encryption. BLE callbacks only copy into a fixed queue; parsing, saved-device writes and upload handling run on the main loop. Line and upload sizes are bounded, offsets and encoded payloads are checked, and queue overflow ends the connection session. USB work per loop is bounded. Allocation guards reject oversized uploads before reserving their buffers.

The two modes share one SD adapter (`sd_card.cpp`) and generated pin settings, with a conservative 4 MHz bus. Image files live in `/etag/images`; probe logs retain `/etag/probe-NNNNNN.json`. No mount operation formats a card. Text is rendered in strips to reduce RAM use. SD image rendering uses a compact 1/2-bit canvas and can reject large images if memory is unavailable; browser preparation is preferable for large panels.

The browser source in `web/` is served directly; there is no duplicate GitHub Pages build. USB uses Web Serial, BLE uses Nordic UART Service with 20-byte writes, and both use newline-delimited JSON. Browser upload/save acknowledgements confirm Cardputer receipt/persistence only. A random Cardputer stage token binds Send to the acknowledged target, page and exact prepared bitstream; reconnecting or editing invalidates the browser's staged identity. These acknowledgements are not tag acknowledgements.

## OpenEPaperLink interoperability

This is an independently authored client of an existing AP's HTTP API. It does not incorporate OEPL source or replace any radio/tag firmware. The [pinned API reference](reference/openepaperlink.md) records upstream behavior, license, and unverified hardware boundaries.

`lib/EtagCore/src/etag/oepl/` implements portable IPv4 endpoint/MAC validation, bounded HTTP framing, multipart construction, baseline JPEG envelope checks, and AP-acceptance semantics. Its nonblocking `Transport` interface supports fake sockets, partial writes, disconnects, cancellation, absolute exchange deadlines, Content-Length/chunked/close-delimited responses, and size caps. This is a separate HTTP transport, not an implementation of the OEPL RF packet definitions or TI debug protocol.

`firmware/programmer/src/oepl/OeplMode` owns Wi-Fi and the shared SD/canvas while menu 4 is active. Only one worker runs; it publishes its result with an atomic release/acquire handoff. UI cancellation waits for that worker before turning Wi-Fi off or changing modes. The worker never draws or accesses the debug wire/RMT. A fresh key event is required after a result/preview appears. Socket sends are nonblocking so retry/deadline ownership stays in the core. AP responses have a 16 KiB cap, JSON receives a depth prefilter followed by cJSON parsing, and records are bounded to 32 per upstream page. Invalid pagination fails explicitly. Wi-Fi credentials are read from the ignored SD settings file, not printed or persisted to NVS by this client. BLE's retained allocation may require a reboot before OEPL if the heap guard refuses a worker.

The Cardputer streams baseline JPEGs up to 512 KiB instead of holding a decoded full-size image in RAM. It checks exact AP metadata dimensions and records the reviewed file's SHA-256. Before POST it rechecks registration/metadata and rehashes the file, then rewinds that same handle. AP rotation/palette settings are preserved. Read/preparation failures do not submit an image; ambiguous POST outcomes never retry automatically.

`tools/oepl.py` is the shared desktop CLI/client. `tools/studio.py` serves `web/` and a same-origin, loopback-only JSON bridge. `web/oepl.js` holds independent AP/tag/artwork state and invalidates asynchronous work when identity changes. These AP records are temporary and never upgrade `profiles/tags` evidence or enter the IR barcode catalog. The Python client accepts HTTP/HTTPS origins, fetches bounded AP metadata/pagination, validates baseline JPEGs up to 2 MiB, and submits an explicit multipart image. Host/Origin checks prevent unrelated websites from using the local bridge; no wildcard CORS or arbitrary forwarded paths are provided.

Only HTTP 200 with the expected positive body is reported as submitted. An AP upload response, RF progress reaching 100%, and a zero pending count each fall short of verified screen refresh. Status remains observational; there is no request-specific upstream job ID. No live gateway/tag operation is part of builds or tests.
