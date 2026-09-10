# OpenEPaperLink integration reference

Reviewed on 2026-09-10 against OpenEPaperLink commit
[`351e687f295dac2723194c80232d34a475e2078b`](https://github.com/OpenEPaperLink/OpenEPaperLink/commit/351e687f295dac2723194c80232d34a475e2078b)
(2026-09-09), resolved using `git ls-remote ... HEAD` and checked in a temporary
checkout. This is a source review, not a test against an access point or tag.
Upstream HTTP behavior is not a versioned interoperability guarantee.

## Scope and attribution

OpenEPaperLink provides access-point software, replacement tag firmware, and its
own radio protocols. The useful integration boundary here is an independently
implemented HTTP client for an existing, configured OEPL access point. This does
not turn the Cardputer's Wi-Fi or infrared emitter into an OEPL tag radio.

The upstream [LICENSE](https://github.com/OpenEPaperLink/OpenEPaperLink/blob/351e687f295dac2723194c80232d34a475e2078b/LICENSE)
is CC BY-NC-SA 4.0. No upstream source, firmware, artwork, or tag-type database is
imported by this reference. Read the applicable license before any future copying;
do not assume this material can be incorporated under this project's code license.
The [README](https://github.com/OpenEPaperLink/OpenEPaperLink/blob/351e687f295dac2723194c80232d34a475e2078b/README.md#L8-L13)
explicitly marks much of its content obsolete, including tags used as RF
coprocessors. Use current source for API behavior and the
[wiki](https://github.com/OpenEPaperLink/OpenEPaperLink/wiki) for current hardware.

## Reading tags

`GET /get_db` returns an object containing `tags`, an array of records, and
optionally `continu`, the next database position. Request `/get_db?pos=N` using
that continuation until it is absent. Page boundaries depend on serialized size
(the source checks for more than 5000 bytes after adding a record), not a fixed
tag count. Only current records are included. The upstream position parameter is
eight bits: reject out-of-range, repeated, or backward continuations rather than
silently wrapping or looping. Deduplicate by MAC and tolerate database changes
between requests; the endpoint does not provide snapshot isolation.

`GET /get_db?mac=0011223344556677` returns the same envelope for one tag; a missing
tag gives an empty array. A malformed MAC can produce HTTP 200 with an `error`
object. Validate the JSON shape as well as HTTP status.

| Field | Interpretation |
| --- | --- |
| `mac`, `alias`, `hwType` | Identity, user label, numeric hardware-type ID. Emitted MACs are 16 uppercase hexadecimal characters, without separators. |
| `hash`, `updatecount`, `updatelast` | AP's recorded transfer version, completed-transfer count, and completion timestamp. These are not the uploaded JPEG's checksum or a request ID. |
| `lastseen`, `nextcheckin`, `nextupdate`, `pending` | Check-in/scheduling timestamps and pending-queue count. Zero pending does not prove a successful transfer. |
| `batteryMv`, `temperature`, `RSSI`, `LQI` | Reported tag telemetry. |
| `contentMode`, `modecfgjson` | Current content mode and configuration encoded as a JSON **string**. |
| `rotate`, `lut`, `invert` | Existing AP-side image settings; preserve unless the user changes them. |
| `isexternal`, `apip`, `capabilities`, `wakeupReason`, `ch`, `ver` | AP association, capabilities, wake reason, channel, and tag firmware version. |

Sources: [HTTP route](https://github.com/OpenEPaperLink/OpenEPaperLink/blob/351e687f295dac2723194c80232d34a475e2078b/ESP32_AP-Flasher/src/web.cpp#L318-L338),
[MAC parsing, pagination, and record serialization](https://github.com/OpenEPaperLink/OpenEPaperLink/blob/351e687f295dac2723194c80232d34a475e2078b/ESP32_AP-Flasher/src/tag_db.cpp#L48-L127).

## Display metadata

Fetch `/tagtypes/XX.json`, where `XX` is the uppercase, two-digit hexadecimal
`hwType` (decimal 1 becomes `01.json`). Read it from the selected AP: the source
repository's `data/tagtypes` directory is a placeholder; upstream metadata lives
under `resources/tagtypes` and is populated on the AP separately. Missing metadata
means unknown dimensions, not permission to guess or to rewrite the AP's files.

Relevant keys are `name`, integer `width`, `height`, `bpp`, `rotatebuffer`, and
`colortable`. The color table is an object whose named values are RGB triples,
not a list of color names. Preserve palette order. Optional `perceptual` provides
the upstream UI's preview palette; firmware conversion uses `colortable`.
`bpp=2` does not distinguish BWR, BWY, and BWRY. Compression and waveform fields
are hardware/firmware capabilities, not client instructions to invent a codec.

The upstream browser uploader prepares JPEGs at metadata `width` by `height`.
The AP applies configured `rotate` and hardware `rotatebuffer` during conversion;
do not swap upload dimensions just because `rotatebuffer` is odd. The HTTP handler
itself does not enforce dimensions, so requiring an exact-sized baseline JPEG is
a deliberate client restriction. A local preview is an approximation of the AP's
palette conversion, dithering, rotation, and physical e-paper result.

Sources: [metadata loader](https://github.com/OpenEPaperLink/OpenEPaperLink/blob/351e687f295dac2723194c80232d34a475e2078b/ESP32_AP-Flasher/src/tag_db.cpp#L397-L455),
[example metadata](https://github.com/OpenEPaperLink/OpenEPaperLink/blob/351e687f295dac2723194c80232d34a475e2078b/resources/tagtypes/01.json),
[JPEG decoding and buffer rotation](https://github.com/OpenEPaperLink/OpenEPaperLink/blob/351e687f295dac2723194c80232d34a475e2078b/ESP32_AP-Flasher/src/makeimage.cpp#L32-L160),
[upstream browser source, gzip](https://github.com/OpenEPaperLink/OpenEPaperLink/blob/351e687f295dac2723194c80232d34a475e2078b/ESP32_AP-Flasher/data/www/main.js.gz).

## Submitting an image

`POST /imgupload` uses multipart form data. Send ordinary fields **before** the
file part because the upload callback reads `mac` when the file begins. The usual
fields are `mac`, optional `dither`, optional `ttl`, and `file` with a JPEG body.
Default `dither` is 1; default `ttl` is 0. Other optional fields include `alias`,
`rotate`, `lut`, `invert`, `contentmode`, `preloadtype`, and `preloadlut`; a simple
image client should omit these rather than unexpectedly change configuration.

The successful handler stores a temporary `.jpg`, defaults content mode to 24,
sets `nextupdate` to zero, and responds with HTTP 200 and text `Ok, saved`.
Missing `mac` returns 400, unknown MAC returns 400, and a non-running AP returns
409. The route also has a generic empty HTTP 200 response path: malformed MACs,
missing files, or other incomplete requests must not be treated as success merely
because the status is 200. Require the positive response text and preflight the
selected tag. File errors can also occur after submission.

Report success as **submitted to AP**, not displayed. The positive response does
not establish JPEG decoding, RF delivery, or a completed display refresh. There
is no request-specific job ID or idempotency key. An ambiguous POST timeout should
remain ambiguous; do not automatically resend and create a second update.

Sources: [route](https://github.com/OpenEPaperLink/OpenEPaperLink/blob/351e687f295dac2723194c80232d34a475e2078b/ESP32_AP-Flasher/src/web.cpp#L312-L316),
[upload callback](https://github.com/OpenEPaperLink/OpenEPaperLink/blob/351e687f295dac2723194c80232d34a475e2078b/ESP32_AP-Flasher/src/web.cpp#L941-L1068).

## Status and completion

The websocket endpoint is `/ws`. Messages can contain `tags` (the same record
envelope as `/get_db`), `sys`, `logMsg`, `errMsg`, or
`upload: {src, current, total}`. Progress is not an acknowledgement: for the block
radio path, reaching `total` is emitted when the last block is handed to the
radio coprocessor, before a tag's transfer-complete report arrives.

On `processXferComplete`, the AP logs the report, updates the hash, increments
`updatecount`, sets `updatelast`, removes the transfer from its queue, and emits
updated tag data. A timeout also clears queued work and emits tag data; therefore
`pending == 0` alone is insufficient. Snapshot counters before submission and
correlate subsequent observations conservatively. Other clients and commands can
change the same records, and transfer completion is not optical verification of
the screen. Polling `/get_db?mac=...` is useful without implementing websocket
support, but cannot recover every transient error message.

Sources: [websocket messages](https://github.com/OpenEPaperLink/OpenEPaperLink/blob/351e687f295dac2723194c80232d34a475e2078b/ESP32_AP-Flasher/src/web.cpp#L40-L74),
[tag messages](https://github.com/OpenEPaperLink/OpenEPaperLink/blob/351e687f295dac2723194c80232d34a475e2078b/ESP32_AP-Flasher/src/web.cpp#L210-L216),
[progress, completion, and timeout processing](https://github.com/OpenEPaperLink/OpenEPaperLink/blob/351e687f295dac2723194c80232d34a475e2078b/ESP32_AP-Flasher/src/newproto.cpp#L409-L535).

## Future firmware design lessons and SES limits

- Keep content preparation, AP queuing, transport, and tag firmware separate.
  Support cannot be created by a profile or by matching an HTTP API alone.
- For future radio work, consider bounded block transfers, selective missing-part
  requests, checksums, versioned content, retries, and explicit timeout states.
  OEPL's [wire declarations](https://github.com/OpenEPaperLink/OpenEPaperLink/blob/351e687f295dac2723194c80232d34a475e2078b/oepl-proto.h#L100-L150)
  describe 4096-byte blocks and part-request masks. These are design references,
  not evidence of compatibility with a stock SES radio.
- Sleep and check-in scheduling belong to model-specific tag firmware. Measure
  current and latency on each PCB; historical README battery figures are not
  validation of this project's hardware.
- Keep initial wired programming distinct from subsequent OTA updates. A future
  OTA design needs exact MCU/capacity/display compatibility, authenticated images,
  power-loss recovery, and independent verification before activation. OEPL's OTA
  feature does not grant stock SES firmware interoperability.

No explicit support for `ERD3-0210-A`, `EDG3-0220-B`, `GL440`, or `CC2510` was
found in the reviewed upstream hardware metadata/definitions or current supported
tag wiki. This is a documented-support limit, not proof that a future port is
impossible. Keep the local SES profiles candidate/unknown until PCB and bench
evidence establishes their individual properties. SiLabs/nRF SWD instructions,
ZBS flashing, and TI CC-Debug are different programming paths.

The separate `prime-axiom/gl440-client-firmware` project is not upstream OEPL
support. Its reviewed main branch targets a 2.6-inch 296x152 GL440 and has empty
radio methods and a protocol stub; its companion AP's OEPL claim concerns the
HTTP/WebSocket interface. It is neither a working stock SES gateway emulator nor
verified firmware for the user's 2.2-inch EDG3 or unidentified ERD3.
[Client radio source](https://github.com/prime-axiom/gl440-client-firmware/blob/main/src/radio/radio.c),
[client protocol source](https://github.com/prime-axiom/gl440-client-firmware/blob/main/src/protocol/protocol.c).
