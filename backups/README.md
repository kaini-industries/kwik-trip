# Original device acquisitions

Per-unit files here are ignored by Git. Keep an additional independent copy before erasing a tag. `tools/etag.py backup` archives two matching main-flash reads and their metadata; it does not read the hardware itself or establish a full-device restore image.

Keep information-page, external-flash and NFC acquisitions separate, with lengths, hashes, acquisition method, source revision, and raw protection status. Preserve original files unchanged. Do not blindly restore lock bits.
