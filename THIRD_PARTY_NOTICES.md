# Licensing and provenance

The etag project source is licensed under GNU GPL version 3 only; see [LICENSE](LICENSE). This includes the combined Cardputer application incorporating TagTinker ADV. When distributing it, retain the notices and provide the corresponding source under GPLv3. The repository's visibility does not waive those obligations.

## TagTinker ADV

- Author/project: [i12bp8 / TagTinker ADV](https://github.com/i12bp8/tagtinker-adv).
- Reviewed/imported revision: [fb8a0669bbdfe77c51bdbd5adbb8cb2ab00f2db2](https://github.com/i12bp8/tagtinker-adv/tree/fb8a0669bbdfe77c51bdbd5adbb8cb2ab00f2db2).
- License: GPL-3.0-only. Upstream SPDX notices are retained.
- Imported components: portable protocol, waveform and image codec; Cardputer application, renderer, storage and transmitter; browser editor; regression tests and protocol notes.
- Local locations: `lib/EtagCore/src/etag/display/`, `firmware/programmer/src/display/`, `web/`, `tests/display/`, and `docs/reference/tagtinker-adv/`.
- Modified for etag on 2026-09-10: integrated wired-console mode, shared generated pin/SD configuration, runtime Advance check, device validation/capabilities, atomic saved-record handling, bounded/validated browser commands, separate USB/BLE sessions, browser save acknowledgements and honest supported controls, build/test integration, and documentation.

The [import manifest](docs/reference/tagtinker-adv/import.json) records original source paths and hashes. The [upstream protocol notes](docs/reference/tagtinker-adv/protocol.md) retain the research context. Upstream credits [furrtek/PrecIR](https://github.com/furrtek/PrecIR) for the underlying infrared research. Neither upstream's compatibility reports nor its model table constitute physical validation of this fork.

## Barcode scanner and build dependencies

The bundled ZXing browser scanner retains its [Apache 2.0 license](web/vendor/ZXING-LICENSE). The M5Stack, Arduino, ESP-IDF, fonts, PlatformIO and test dependencies retain their own licenses and notices; they are resolved through the pinned build configuration. This notice does not replace their terms.

## Reference material

The project license applies to project source; it does not relicense third-party datasheets, schematics, photographs or archived research under `hardware/` and `docs/reference/`. Their original attribution, provenance and rights remain applicable. Original reference hashes remain protected by the migration manifest.
