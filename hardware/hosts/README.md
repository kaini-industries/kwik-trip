# Generic host boards

`config/hosts.json` is the wiring source of truth. Each PlatformIO environment must have a matching host entry; pre-build checks reject pin conflicts and board-ID mismatches.

| Host | Module baseline | DD / DC / RESET |
| --- | --- | --- |
| esp32-devkit | ESP32-WROOM, 4 MB flash | 23 / 19 / 33 |
| esp32-s3-devkit | DevKitC-1 N8, no PSRAM | 4 / 6 / 15 |

Use a verified regulated target supply and common ground. Do not repurpose USB, flash, strapping, or input-only GPIO. A different module, attached peripheral, or board revision may impose additional conflicts; record those in a new host profile. These mappings have build validation only, not physical bench validation.
