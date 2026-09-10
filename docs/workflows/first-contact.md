# First contact: wired CC2510 probe

This procedure covers the currently implemented TI CC2510 interface. For any new tag, begin with [adding a tag to your collection](add-a-tag.md) to identify its protocol and hardware revision. Other MCU families need their own adapter and backend before active communication.

1. Identify the MCU marking, PCB revision, battery supply, and panel label. Photograph both sides and fill in the pad map. A case label is not a verified MCU identity.
2. With the battery removed, map the debug nets using continuity. Verify regulator output and target voltage before connecting GPIO. Do not supply a coin cell and an external rail simultaneously.
3. Upload the selected ESP32 programmer build with the tag disconnected. Confirm the host, GPIO listing, and idle state. Use `pins` and compare with the actual connector orientation.
4. Wire ground, regulated power and confirmed debug nets. The CC2510 RESET line must have a target-side pull-up. The current direct GPIO adapter is intended for a target verified for 3.3 V operation; lower-voltage targets require appropriate level handling.
5. Select the matching profile, then type `probe confirmed`. This temporarily enters debug mode and resets the tag afterward. No flash command is sent. Profiles with an unknown or unimplemented protocol cannot activate this interface; do not select a different model merely to bypass that restriction.
6. Repeat identification across reconnects and cold power cycles. `0x81xx` is CC2510 model plus revision; it does not establish F8/F16/F32 capacity. A reported lock is different from an unstable or missing response.
7. Run `sd` then `save` on the Advance to retain probe metadata, or save the USB serial output. SD failure leaves the card unformatted and probing remains usable.
8. Release/disconnect programmer signals before removing target power or restoring its battery. Record results in a per-unit inventory file.

| Command | Behavior |
| --- | --- |
| `help` | List commands and probing prerequisite |
| `about` | Host and scaffold version |
| `pins` | DD/DC/RESET assignments; no target I/O |
| `profiles` | Catalog and selected profile |
| `select <profile-id>` | Select the connected tag's metadata; replace the placeholder with its actual ID |
| `probe confirmed` | Explicit CC2510 ID/status probe; normal boot restored afterward |
| `sd` | Mount Cardputer SD; no formatting |
| `save` | Create a new log for the latest probe; not a firmware backup |

Readout and erase/program are next milestones, not hidden commands. Do not use the old reference flasher's write workflow as though the scaffold has already validated it. If identification fails, measure power and continuity and inspect signals before changing wiring or removing components.
