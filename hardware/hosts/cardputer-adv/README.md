# Cardputer Advance adapter

This assignment is specific to the Advance. The source of truth is [config/hosts.json](../../../config/hosts.json). No tag pad positions have been physically verified.

| Function | Net label | Position in M5Stack website EXT table |
| --- | --- | --- |
| DD | G4 | 3 |
| DC | G6 | 5 |
| RESET_N | G15 | 14 |
| Ground | GND | 4 |
| Regulator input | 5VOUT | 6 |
| Tag VDD | Regulated output from adapter | Separate terminal |

The website uses physical table positions; schematic P3 uses a different terminal-numbering scheme. Follow **net labels and connector orientation**, not a bare pin number. The [saved schematic](reference/cardputer-adv-v1.0-schematic.pdf) and [official documentation](https://docs.m5stack.com/en/core/Cardputer-Adv) are the references.

The EXT header's supply is 5 V. Add a regulator and measure its output before attaching a 3.3 V-compatible target. The firmware neither controls nor senses target power. GPIO must not drive an unpowered target. RESET is open-drain, so verify the target-side pull-up; DD/DC remain direct 3.3 V logic and are not automatically level-shifted.

Prototype parts: 2×7 breakout, regulator with its required decoupling, manual target-power switch, short flexible leads, labelled tag connector, multimeter, and strain relief. Prefer a current-limited bench supply for initial investigation. Disconnect other caps/accessories to avoid shared pin contention.

Reserved buses remain available for onboard peripherals: keyboard/audio/IMU use G8/G9, and microSD uses G12/G14/G39/G40. This application uses the SD bus only after `sd`. The display is managed by M5Stack's libraries. Runtime identification must report Cardputer Advance before probing is enabled.
