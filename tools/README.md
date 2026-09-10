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
