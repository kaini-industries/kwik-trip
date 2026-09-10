# Sources and third-party code

The implementation uses the public TI debug-interface specification and separately authored portable protocol logic. Legacy flasher/web code has not been vendored into `lib/`. PlatformIO downloads its declared dependencies into ignored `.pio` directories, retaining upstream licenses.

| Reference | Revision / use |
| --- | --- |
| [ESP_CC_Flasher](https://github.com/atc1441/ESP_CC_Flasher) | Reviewed `c61a4183aaf99ece83ad208a0ab90dfe46fdd24a`; protocol and defect research |
| [imagotag-hack](https://github.com/andrei-tatar/imagotag-hack) | Reviewed `dc0145f1e48d0131240a57750aaa8514d024423c`; hardware and memory-layout research |
| [Frickl-EPaper](https://codeberg.org/section77/Frickl-EPaper) | Reviewed `250758ddd2f9c8650d5385cf9d8f932ab09357e7`; display and fixture references |
| [TI CC2510 datasheet](https://www.ti.com/lit/ds/symlink/cc2510.pdf) | Debug command framing, model/status, reset and memory map |
| [M5Stack Advance documentation](https://docs.m5stack.com/en/core/Cardputer-Adv) | Host pin map and peripherals |
| [M5Cardputer](https://github.com/m5stack/M5Cardputer) | Commit-pinned library; upstream 1.2.0 tag has a 1.1.1 package manifest |
| [M5Unified](https://github.com/m5stack/M5Unified) / [M5GFX](https://github.com/m5stack/M5GFX) | Exact versions in host configuration |
| [IRremote](https://github.com/Arduino-IRremote/Arduino-IRremote) | Pinned dependency required by M5Cardputer's package; unused by this application |
| [PlatformIO MCS51 builder](https://github.com/platformio/platform-intel_mcs51) | Board size configuration and SDCC link settings |

The saved M5Stack schematic is publisher reference material, not an etag-authored hardware design. Check each upstream license before distributing copied code or combining it into firmware. A project-wide license has not been chosen by the owner.
