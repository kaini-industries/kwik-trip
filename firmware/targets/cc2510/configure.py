Import("env")
from SCons.Script import COMMAND_LINE_TARGETS

if any(target in COMMAND_LINE_TARGETS for target in ("upload", "uploadfs", "program")):
    raise RuntimeError("CC2510 upload is intentionally not wired to a serial uploader. Package a verified image and use a validated TI debug programmer.")
# Fast IRAM and slow XRAM are different address spaces, not 4 KB of generic IRAM.
env.Append(LINKFLAGS=["--xram-loc", "0xF000", "--code-loc", "0x0000"])
