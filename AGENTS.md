# Working in etag

- Read README.md and docs/architecture.md before changing firmware structure.
- The root PlatformIO project builds ESP32 hosts. firmware/targets/cc2510 is a separate SDCC project; keep tag and host binaries distinct.
- Use .venv/bin/python and .venv/bin/pio (Python 3.12), or their activated-environment equivalents. Run make validate for changes spanning profiles, tooling, or firmware.
- config/hosts.json and profiles/tags/*.json are the source of hardware metadata. Per-environment headers are generated into .pio; do not hand-edit generated files or duplicate pin assignments in protocol code.
- Keep hypotheses marked candidate/unknown. Only mark hardware verified when actual measurements and PCB evidence are recorded. Never infer target capacity from CC2510 chip ID alone.
- Keep protocol logic in lib/EtagCore independent of Arduino; test failure paths with a fake DebugWire.
- Builds and tests must not upload to attached devices or issue target erase/program operations. Follow an explicit user request when hardware operations are requested.
- Preserve original reference files and their hashes. Add corrections in current documentation rather than rewriting archived research.
- Do not commit .pio, .venv, firmware dumps, personal unit inventory, Wi-Fi credentials, or downloaded toolchains.
- Pin changed dependencies, run affected builds, and report compilation versus hardware validation separately.
