# Target profiles

`tags/*.json` is the runtime catalog, validated by `tools/etaglib.py` and compiled into each host build. Unknown values are `null`; `unknown`, `candidate`, and `verified` describe evidence confidence, not a successful compilation.

The catalog is intended for a collection of tags across manufacturers. Use one profile per hardware revision and a separate inventory record for each unit. The initial model records are research examples; they do not restrict which models can be documented. Follow [adding a tag](../docs/workflows/add-a-tag.md) for the complete process. Profile changes require rebuilding and reinstalling the host firmware; SD profile loading is not implemented.

`protocol` accepts `unknown`, `ti_cc_debug`, `swd`, `uart_bootloader`, and `spi_flash`. Only the CC2510 identification/status implementation is active today. Other entries can document hardware but cannot activate a backend. MCU chip IDs may have different widths across protocols.

Hardware confidence and supported operations are separate. Setting `verified` does not add readout, programming, display support, or a protocol implementation. Record bench results for each operation and revision before claiming compatibility.

A verified profile requires complete MCU identification/capacity, measured power, PCB revision and existing evidence files. Panel fields can remain unknown while validating the programmer, but must be completed before claiming display support. A positive chip response alone must not set `verified`.

Use `tools/etag.py new-profile` to create a complete template. Schema and semantic checks are deliberately strict so typos do not silently become defaults. Per-unit serial numbers, sessions and private observations belong in ignored `hardware/inventory/*.local.json`; a profile describes a hardware revision shared by multiple units.
