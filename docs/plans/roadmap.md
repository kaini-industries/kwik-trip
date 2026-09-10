# Implementation roadmap

The goal is a Cardputer Advance workbench for a collection of e-tags: identify a unit, preserve its original firmware, select a compatible custom image, program it, and verify the result. Support grows per MCU protocol and hardware revision. Other ESP32 hosts share the same core where their hardware permits.

The Cardputer application already exists, with keyboard/display controls, USB console, profile selection, and SD probe logging. Its first active tag interface is a wired CC2510 identification/status probe. The [original model-specific plan](cardputer-advance-flashing-plan.md) remains research history; this roadmap describes the broader project.

| Milestone | State | Exit condition |
| --- | --- | --- |
| Workspace, pinned toolchains and reference organization | Implemented | Catalog validation, software tests and build matrix pass |
| GPIO/USB/Cardputer diagnostic application | Implemented; bench validation pending | Keyboard, SD, idle pins and repeatable chip/status responses verified physically |
| Catalog the user's tag collection | Templates and two initial research profiles | One record per hardware revision, separate unit inventory, MCU/voltage/pad/panel evidence recorded |
| Protocol dispatch and capabilities for additional MCU families | CC2510 probe only | Each new backend has explicit operations, suitable transport/pin configuration, simulated failure tests, and bench results |
| Internal flash and information-page readout | Not implemented | Two matching reads, complete error/status handling, hash and provenance records |
| External flash/NFC acquisition | Not implemented | Hardware-specific access and independent dumps |
| Erase/program/verify workflow | Not implemented | Validated package, confirmed target/capacity, explicit erase, bounded errors, exact full readback |
| Complete on-device programming workflow | Probe console and logs only | Cardputer can acquire backups, select and validate compatible SD images, show progress, and retain actionable failure logs |
| Board-specific display firmware and sleep | Not implemented | Correct pattern, polarity/orientation, cold boot and measured idle current |
| Keyed pogo fixture and multiple-unit workflow | Templates only | Repeated operation on a second confirmed revision |
| Target-specific radio or NFC content delivery | Future | Compatible transport and hardware identified, working recovery, complete payload validation and staged updates |
| OTA tag firmware | Future | Recoverable bootloader and tested power-loss behavior |

The next software work is a bounded readout backend and acquisition workflow for the first confirmed MCU family. Keep readout/backup distinct from destructive programming. A CC2510 reader must refresh protection state correctly, restore memory selection on errors, preserve information-page data independently, and report transport failures rather than assuming debug lock. Other families need their own memory and protection rules.

The next hardware work is inventorying the available collection and inspecting one representative of each distinct revision. The existing photographs can seed that inventory, but do not limit it. Follow [adding a tag](../workflows/add-a-tag.md), record measured pad maps, and choose the first backend based on confirmed hardware. Do not invent measurements or mark an entire family supported from one working unit.

Tag application development proceeds separately: a working programming interface does not supply a display driver or make one firmware image interchangeable across panels. Extend the current address-zero package format when a target needs different flash regions or bootloader offsets. Treat a second MCU family as an opportunity to validate the shared design, rather than assuming the TI-specific adapter already abstracts every interface.
