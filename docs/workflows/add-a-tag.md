# Add an e-tag to your collection

Start with the hardware you have. This workspace can hold records for different manufacturers and MCU families, while the Cardputer provides a shared user interface. Each device needs an evidence-backed profile, suitable wiring, and an implemented protocol before communication is possible.

## Record the model, revision, and individual unit

Use one profile for each distinct hardware revision. Several identical units can share it; a different MCU, panel, or PCB revision may need another profile. Keep unit serial numbers, acquisition notes, and backup locations in separate local inventory records.

From the repository root, replace `vendor-model-reva` with your identifier:

```sh
.venv/bin/python tools/etag.py new-profile vendor-model-reva --output profiles/tags/vendor-model-reva.json
```

Create `hardware/tags/vendor-model-reva/` with exterior/PCB photos and a copy of the [pad-map template](../../hardware/templates/pad-map.md). Copy the [inventory example](../../hardware/inventory/unit.example.json) to `hardware/inventory/<unit-id>.local.json`, and set its `profile_id` to the new profile. Local inventory records are ignored by Git.

Record the MCU's full marking, PCB revision, voltage, accessible debug nets, and panel markings. Add paths to the evidence files in the profile. Keep missing facts as `null` and leave the protocol `unknown` until evidence supports a family. Case labels and screen dimensions alone do not establish compatibility.

## Determine what this project can do with it

| Evidence and implementation | Available next step |
| --- | --- |
| MCU/protocol unknown | Record and investigate hardware; no active probe |
| Confirmed CC2510 with verified wiring and compatible supply | Use the existing identification/status probe after the bench prerequisites are met |
| Other TI MCU or another protocol family | Implement and validate an appropriate backend; the current probe only accepts CC2510 |
| Known hardware, but no flash reader/writer | Keep acquisition/programming marked pending; no current profile enables those commands |
| Same MCU, different panel or wiring | Validate that revision separately and provide matching tag firmware |

Profile status (`unknown`, `candidate`, `verified`) describes the strength of recorded hardware evidence. It does not represent protocol support or guarantee a working programmer. Even a verified profile needs an implementation for the requested operation and a bench result showing that operation succeeds.

The schema can record `ti_cc_debug`, `swd`, `uart_bootloader`, and `spi_flash`. Only the CC2510 identification/status implementation is active. Other protocol entries are compiled as unsupported. The current host pin schema and GPIO adapter use TI DD/DC/RESET signals; a new interface needs its own transport and pin configuration rather than reusing those labels blindly.

## Make the profile available on the Cardputer

Validate and rebuild after updating the profile and its evidence:

```sh
.venv/bin/python tools/etag.py check
.venv/bin/pio run -e cardputer-adv
```

Follow the root [installation steps](../../README.md#start-with-the-cardputer-advance) with the tag disconnected. On the Cardputer, run `profiles`, then `select vendor-model-reva` using your actual identifier. Selection alone does not activate a tag interface. The catalog is compiled into the application; copying JSON to the SD card does not update it.

For a confirmed CC2510, continue with [first contact](first-contact.md) and the [Cardputer adapter guide](../../hardware/hosts/cardputer-adv/README.md). For another family, use [CONTRIBUTING.md](../../CONTRIBUTING.md) to implement and test its communication path before connecting it. Record physical results with the [bench-validation template](../../hardware/templates/bench-validation.md).

## Work toward reprogramming

The intended sequence is identification, original-firmware acquisition, a matching custom image, deliberate programming, and complete readback verification. The Cardputer currently stops at identification/status and probe logging. The [roadmap](../plans/roadmap.md) tracks the remaining implementation.

The computer tools can archive two matching dumps produced by an independent reader, and can package an image for a verified profile. They do not acquire dumps or install images themselves. Main flash, information pages, external storage, and NFC data may require separate acquisitions. Build the custom tag image in an appropriate [target firmware project](../../firmware/targets/README.md), with a memory layout and display driver that match the actual tag.
