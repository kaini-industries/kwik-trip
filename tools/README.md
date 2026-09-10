# Local tools

`etag.py` and `etaglib.py` need only Python's standard library. They never connect to hardware. Run them from the repository root; argument file paths are relative to your working directory. Profile evidence paths are relative to the repository root.

```sh
.venv/bin/python tools/etag.py doctor
.venv/bin/python tools/etag.py profiles
.venv/bin/python tools/etag.py check
.venv/bin/python tools/etag.py new-profile vendor-model --output profiles/tags/vendor-model.json
```

`check` validates strict profile fields, known/unknown values, evidence paths, unique IDs, legal/conflict-free host GPIOs, original reference hashes, and local Markdown links. `pio_prepare.py` performs profile and host validation again during embedded builds and generates the C++ catalog. Archived legacy notes are excluded from link linting because their historical content is preserved.

**Firmware packages**

Once a target profile is physically verified and its capacity is recorded:

```sh
.venv/bin/python tools/etag.py package path/to/tag.hex --profile profiles/tags/vendor-model.json --output artifacts/vendor-model-build-001
.venv/bin/python tools/etag.py verify artifacts/vendor-model-build-001
```

These example paths must be replaced. The two initial profiles deliberately cannot be packaged. `.hex`/`.ihx` inputs require checksums, EOF and data at address zero. The converter rejects overlap, unsupported record types, address wrap and out-of-capacity data. Raw `.bin` is treated as beginning at zero. Gaps and the unused tail are filled with `0xFF` to the recorded capacity.

The output contains `firmware.bin` and `manifest.json` with target identity, profile snapshot, lengths, load address and SHA-256. Verification checks the exact binary and profile agreement. The hash detects corruption, not authorship; this is not a signature or proof that a program is appropriate for a panel. The manifest is written last. Existing output directories are never overwritten. Verification needs the profile's evidence files available in this workspace.

**Backup records**

After a separate validated reader has produced two independent dumps:

```sh
.venv/bin/python tools/etag.py backup --first artifacts/read-1.bin --second artifacts/read-2.bin --profile profiles/tags/vendor-model.json --unit bench001
```

The command compares both files, requires recorded capacity, rejects uniform empty-looking dumps, and creates a new `backups/bench001/` with `main.bin` and metadata. It cannot prove the files came from independent physical reads; acquisition provenance remains part of the bench record. This is main flash only, not a full-device restore package. Keep information-page, external flash and NFC acquisitions separately. Existing unit directories are never overwritten.

## OpenEPaperLink access-point client

`oepl.py` uses Python's standard library to communicate with an **existing OpenEPaperLink AP and tags already registered there**. This is a separate display-content route from the infrared editor and wired diagnostics. It does not flash tags, emulate a Vusion gateway, or establish support for either SES research profile. See the [workflow](../docs/workflows/openepaperlink.md) and [pinned API reference](../docs/reference/openepaperlink.md).

Use an explicit AP origin and the MAC reported by that AP; replace these example addresses before running them:

```sh
.venv/bin/python tools/oepl.py tags --ap http://192.168.1.50
.venv/bin/python tools/oepl.py status --ap http://192.168.1.50 --mac 00000123456789AB
.venv/bin/python tools/oepl.py upload artwork.jpg --ap http://192.168.1.50 --mac 00000123456789AB --dither 0
```

`tags` and `status` are read-only. `upload` explicitly submits an image for that tag and changes its AP-managed content. It first rechecks registration and AP-reported display dimensions. The command accepts a complete baseline JPEG, 8-bit grayscale or three-component color, at most 2 MiB and 2048 × 2048 pixels. The JPEG must match the AP metadata's width and height exactly. Progressive JPEGs, missing metadata, unknown MACs, malformed image framing, and dimension mismatches stop before an upload. Structural JPEG validation does not decode its pixel coefficients; successful decompression still depends on the AP.

The AP applies its own palette, dithering, rotation and display settings. A `bpp` value alone does not identify its colors. `--dither 0` preserves hard edges such as text; `--dither 1` requests AP dithering for photos. The client does not overwrite the AP's alias, rotation, inversion, or lookup-table settings. Submitting custom content can replace a tag's current scheduled content mode, as in the AP's own uploader.

Output is JSON; errors use stderr and exit status 1. Tag fields are normalized to `mac`, `alias`, `hwType`, `width`, `height`, `bpp`, `batteryMv`, `rssi`, `lastseen`, and `pending`. Missing values are `null`. These are **AP-reported operational facts**, never hardware verification in `profiles/tags`. Metadata is fetched from that AP's `/tagtypes/XX.json`, with uppercase hexadecimal type IDs, rather than guessed from a model name. Missing metadata leaves display fields unknown and adds `metadataError`. No credentials, unit inventory, or addresses are saved.

Pagination follows `/get_db`'s `continu` value and rejects duplicate records, backward continuations, and values beyond the pinned AP's 8-bit position limit. The client checks a 30-second operation deadline between requests and response-body reads, and uses an 8-second socket timeout. DNS resolution follows operating-system limits; slow response headers can exceed the operation deadline. Responses are bounded to 256 KiB. The URL must be `http(s)://host[:port]`; credentials, paths, queries, fragments, redirects, compressed responses, ambiguous transfer framing and environment proxy settings are not accepted. HTTPS uses normal certificate verification.

Only HTTP 200 with the AP's expected `Ok, saved` response produces `status: "submitted"`. **`displayConfirmed` always remains `false`.** An empty 200, network loss, or timeout after submission is an unknown result, and uploads are never automatically retried. Check the AP and the physical display before repeating an ambiguous submission. Neither zero pending items nor a progress bar is sufficient display confirmation.

## Browser Studio bridge

```sh
make studio
# Or choose a different local port:
.venv/bin/python tools/studio.py --port 8001
```

The server serves only `web/` and binds to `127.0.0.1`. Open the printed localhost address and choose OpenEPaperLink in the editor. The browser communicates with this same-origin bridge, which makes the AP requests; it does not need cross-origin access to the AP. USB and BLE editing continue to use the existing browser APIs.

The bridge validates its local Host header and requires an exactly matching Origin header on JSON POST requests. It does not offer remote access, directory listings, proxy endpoints, a wildcard CORS policy, or access to repository files outside `web/`. At most eight connections run concurrently. Requests are limited to 3 MiB, images to 2 MiB, and request body reads have an 8-second deadline. Stop it with Ctrl-C when done.

The browser contract is:

| POST path | JSON request | JSON response |
| --- | --- | --- |
| `/api/oepl/tags` | `{ap}` | `{ap, tags, metadataError?}` |
| `/api/oepl/status` | `{ap, mac}` | `{ap, tag, metadataError?}` |
| `/api/oepl/upload` | `{ap, mac, image, dither}` | `{ap, mac, status: "submitted", displayConfirmed: false, message}` |

`image` is bare base64 JPEG data, without a data-URL prefix. `dither` is integer 0 or 1. Error replies use an appropriate 4xx/502 status and `{error}`. There are no background uploads or automatic retries.

Tests exercise the real HTTP framing against **loopback mock APs only**:

```sh
.venv/bin/python -m unittest discover -s tests -p 'test_oepl.py' -v
.venv/bin/python -m unittest discover -s tests -p 'test_studio.py' -v
```

These tests cover malformed responses, pagination, missing metadata, image validation, upload acknowledgement semantics, redirects, timeout/failure paths, Host/Origin guards, and request bounds. A development sandbox must allow binding loopback sockets to run them. They never contact an actual gateway or transmit to a physical tag.
