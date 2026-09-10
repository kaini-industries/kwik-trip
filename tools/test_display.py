#!/usr/bin/env python3
"""Run the imported protocol suite and etag integration regressions without hardware."""
import os
from pathlib import Path
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
CORE = ROOT / "lib/EtagCore/src"
DISPLAY = CORE / "etag/display"

def main():
    compiler = shlex.split(os.environ.get("CXX", "c++"))
    sources = sorted(str(p) for p in DISPLAY.glob("*.cpp"))
    with tempfile.TemporaryDirectory(prefix="etag-display-tests-") as directory:
        for name in ("protocol", "reliability", "integration"):
            output = str(Path(directory) / name)
            args = compiler + ["-std=c++17", "-Wall", "-Wextra", "-Werror", "-g",
                               "-fsanitize=address,undefined", "-fno-omit-frame-pointer",
                               f"-I{DISPLAY}", f"-I{CORE}"]
            if name == "integration":
                args += ["-DETAG_CARDPUTER_ADV=1", "-Itests/display/fakes",
                         "-Ifirmware/programmer/src/display",
                         "firmware/programmer/src/display/TargetStore.cpp"]
            subprocess.run(args + [f"tests/display/{name}.cpp", *sources, "-o", output],
                           cwd=ROOT, check=True)
            subprocess.run([output], cwd=ROOT, check=True)
            print(f"PASS: {name}", flush=True)
        allocation = str(Path(directory) / "allocation")
        subprocess.run(compiler + ["-std=c++17", "-Wall", "-Wextra", "-Werror", "-g",
                         f"-I{DISPLAY}", "tests/display/allocation.cpp",
                         str(DISPLAY / "ImageCodec.cpp"), "-o", allocation],
                       cwd=ROOT, check=True)
        subprocess.run([allocation], cwd=ROOT, check=True)
        print("PASS: allocation", flush=True)
    subprocess.run(["node", "tests/display/web.cjs"], cwd=ROOT, check=True)

if __name__ == "__main__":
    main()
