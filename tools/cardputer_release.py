"""Build, verify and copy the curated Cardputer binaries. Never opens a serial port."""
import argparse
import hashlib
import importlib.metadata
import json
from pathlib import Path
import shutil
import struct
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
RELEASE = Path("firmware/releases/cardputer-adv")
FACTORY = "cardputer-adv-factory.bin"
APP = "cardputer-adv-app.bin"
FILES = (FACTORY, APP, "manifest.json", "SHA256SUMS")
SETTINGS = {"chip": "esp32s3", "board": "m5stack-stamps3", "flash_size": "8MB",
            "flash_mode": "dio", "flash_freq": "80m"}
# Distribution contract: changing the layout requires reviewing all flashing instructions.
OFFSETS = (0x0, 0x8000, 0xE000, 0x10000)
ESPTOOL_VERSION = "5.4.0"
SOURCE_FILES = (
    ".python-version", "platformio.ini", "requirements-dev.txt", "requirements-lock.txt",
    "config/hosts.json", "tools/pio_prepare.py", "tools/etaglib.py",
    "tools/pio_release.py", "tools/cardputer_release.py",
)
SOURCE_DIRECTORIES = (
    "firmware/programmer/src", "firmware/programmer/include", "lib",
    "profiles/tags", "config/platformio",
)


def require(condition, message):
    if not condition:
        raise ValueError(message)


def sha(data):
    return hashlib.sha256(data).hexdigest()


def write_utf8_lf(path, text):
    """Write release metadata without host newline or locale conversion."""
    require("\r" not in text, f"{path.name} contains non-canonical line endings")
    path.write_bytes(text.encode("utf-8"))


def read_utf8_lf(path):
    """Read metadata only when its checked bytes are canonical UTF-8/LF."""
    data = path.read_bytes()
    require(not data.startswith(b"\xef\xbb\xbf") and b"\r" not in data,
            f"{path.name} must use UTF-8 without BOM and LF line endings")
    try:
        return data.decode("utf-8")
    except UnicodeDecodeError as error:
        raise ValueError(f"{path.name} must use UTF-8") from error


def source_inputs(root):
    """Hash build inputs, including new/deleted files, independently of build timestamps."""
    paths = {root / path for path in SOURCE_FILES}
    for directory in SOURCE_DIRECTORIES:
        paths.update(p for p in (root / directory).rglob("*")
                     if p.is_file() and p.suffix not in (".md", ".pyc")
                     and not any(part.startswith(".") for part in p.relative_to(root).parts)
                     and p.name != "local.ini")
    return {p.relative_to(root).as_posix(): sha(p.read_bytes()) for p in sorted(paths)}


def is_source_input_path(path):
    """Apply the source manifest exclusions to a current or deleted Git path."""
    relative = Path(path)
    normalized = relative.as_posix()
    if normalized in SOURCE_FILES:
        return True
    if relative.suffix in (".md", ".pyc") or relative.name == "local.ini" or any(
            part.startswith(".") for part in relative.parts):
        return False
    return any(normalized == directory or normalized.startswith(directory + "/")
               for directory in SOURCE_DIRECTORIES)


def changed_source_inputs(root):
    """Return modified, added, deleted, and untracked build inputs relative to HEAD."""
    pathspecs = (*SOURCE_FILES, *SOURCE_DIRECTORIES)
    tracked = subprocess.check_output(
        ["git", "diff", "--no-renames", "--name-only", "-z", "HEAD", "--", *pathspecs],
        cwd=root,
    )
    untracked = subprocess.check_output(
        ["git", "ls-files", "--others", "-z", "--", *pathspecs], cwd=root)
    paths = {
        path.decode("utf-8", errors="surrogateescape")
        for path in (tracked + untracked).split(b"\0") if path
    }
    return tuple(sorted(path for path in paths if is_source_input_path(path)))


def validate_esp_image(data):
    # The pinned esptool loader parses segments, but digest/checksum mismatches
    # are only printed by its CLI. Explicit comparisons make CI fail on corruption.
    from esptool.bin_image import LoadFirmwareImage
    # esptool 5's public ImageSource API accepts bytes directly. Passing a bare
    # BytesIO is ambiguous with named file inputs in parts of the v5 toolchain.
    image = LoadFirmwareImage("esp32s3", data)
    require(image.chip_id == 9, "Image is not for ESP32-S3")
    require(image.checksum == image.calculate_checksum(), "ESP image checksum mismatch")
    require(image.append_digest and image.stored_digest == image.calc_digest,
            "ESP image SHA256 mismatch")
    require(image.data_length + 32 == len(data), "Truncated image or unexpected trailing data")


def validate_factory(factory, app, components):
    require(tuple(c["offset"] for c in components) == OFFSETS, "Unexpected flash offsets")
    end = 0
    for c in components:
        offset, size = c["offset"], c["size"]
        require(size > 0 and offset >= end and offset + size <= len(factory),
                "Missing, overlapping or truncated flash component")
        require(factory[end:offset] == b"\xff" * (offset - end), "Invalid factory gap padding")
        require(sha(factory[offset:offset + size]) == c["sha256"], "Component SHA256 mismatch")
        end = offset + size
    require(end == len(factory) <= 0x800000, "Invalid factory image size")
    require(factory[0x10000:] == app, "Factory and Launcher application images differ")
    validate_esp_image(factory[:components[0]["size"]])
    validate_esp_image(app)
    # PlatformIO's QIO Arduino bootloader starts in DIO/80MHz/8MB mode.
    require(factory[2:4] == b"\x02\x3f", "Wrong bootloader flash settings")
    table = factory[0x8000:0x8000 + components[1]["size"]]
    entries, md5_seen = [], False
    for pos in range(0, len(table), 32):
        entry = table[pos:pos + 32]
        if entry[:2] == b"\xeb\xeb":
            require(entry[16:] == hashlib.md5(table[:pos]).digest(), "Partition MD5 mismatch")
            md5_seen = True
            break
        require(entry[:2] == b"\xaa\x50", "Invalid partition table entry")
        _, kind, subtype, offset, size, label, flags = struct.unpack("<HBBII16sI", entry)
        entries.append((kind, subtype, offset, size, label.rstrip(b"\0"), flags))
    require(md5_seen, "Missing partition table MD5")
    expected = [
        (1, 2, 0x9000, 0x5000, b"nvs", 0), (1, 0, 0xE000, 0x2000, b"otadata", 0),
        (0, 16, 0x10000, 0x330000, b"app0", 0), (0, 17, 0x340000, 0x330000, b"app1", 0),
        (1, 130, 0x670000, 0x180000, b"spiffs", 0), (1, 3, 0x7F0000, 0x10000, b"coredump", 0),
    ]
    require(entries == expected, "Partition layout changed; review packaging and flashing instructions")
    require(len(app) <= 0x330000, "Application exceeds its partition")
    require(factory[0x9000:0xE000] == b"\xff" * 0x5000, "Factory NVS must be erased")
    # Launcher identifies merged files by this marker; raw apps must not look merged.
    require(app[0x8000:0x8003] != b"\xaa\x50\x01", "Ambiguous Launcher application image")


def verify(directory, root=ROOT):
    manifest = json.loads(read_utf8_lf(directory / "manifest.json"))
    require(manifest["schema_version"] == 1 and manifest["settings"] == SETTINGS,
            "Unexpected release target/settings")
    require(manifest["source_inputs"] == source_inputs(root),
            "Committed firmware is stale: commit build inputs, then run make release-cardputer")
    expected_sums = ""
    for name in (FACTORY, APP, "manifest.json"):
        data = (directory / name).read_bytes()
        expected_sums += f"{sha(data)}  {name}\n"
        if name != "manifest.json":
            require(manifest["files"][name] == {"size": len(data), "sha256": sha(data)},
                    f"Release file damaged: {name}")
    require(read_utf8_lf(directory / "SHA256SUMS") == expected_sums, "SHA256SUMS mismatch")
    validate_factory((directory / FACTORY).read_bytes(), (directory / APP).read_bytes(),
                     manifest["components"])
    return manifest


def merge_factory(output, images, settings):
    """Merge PlatformIO components with the pinned esptool command contract."""
    args = [sys.executable, "-m", "esptool", "--chip", settings["chip"], "merge-bin",
            "--output", str(output), "--target-offset", "0x0"]
    for option in ("flash_mode", "flash_freq", "flash_size"):
        args += ["--" + option.replace("_", "-"), settings[option]]
    for offset, path in images:
        args += [hex(offset), str(path)]
    subprocess.run(args, check=True)


def package(root, directory, images, settings):
    require(settings == SETTINGS, "Unsupported Cardputer distribution settings")
    images = sorted(images)
    require(tuple(offset for offset, _ in images) == OFFSETS, "Unexpected PlatformIO upload images")
    require(importlib.metadata.version("esptool") == ESPTOOL_VERSION,
            "Install pinned requirements-dev.txt")
    inputs = source_inputs(root)
    revision = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=root, text=True).strip()
    dirty = bool(changed_source_inputs(root))
    directory.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(dir=directory.parent) as temporary:
        out = Path(temporary)
        merge_factory(out / FACTORY, images, settings)
        shutil.copyfile(images[-1][1], out / APP)
        factory = (out / FACTORY).read_bytes()
        # merge_bin can update the bootloader header and digest. Hash final bytes.
        components = [{"offset": offset, "size": path.stat().st_size,
                       "sha256": sha(factory[offset:offset + path.stat().st_size])}
                      for offset, path in images]
        manifest = {
            "schema_version": 1, "settings": settings, "source_commit": revision,
            "source_dirty": dirty, "source_inputs": inputs,
            "tools": {name: importlib.metadata.version(name) for name in ("platformio", "esptool")},
            "components": components,
            "files": {name: {"size": (out / name).stat().st_size, "sha256": sha((out / name).read_bytes())}
                      for name in (FACTORY, APP)},
            "hardware_validation": "pending",
        }
        write_utf8_lf(out / "manifest.json", json.dumps(manifest, indent=2) + "\n")
        write_utf8_lf(out / "SHA256SUMS", "".join(
            f"{sha((out / name).read_bytes())}  {name}\n"
            for name in (FACTORY, APP, "manifest.json")))
        verify(out, root)
        directory.mkdir(exist_ok=True)
        for name in FILES:
            shutil.copyfile(out / name, directory / name)
    print(f"Verified Cardputer factory and Launcher images: {directory}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("command", choices=("verify", "publish"))
    parser.add_argument("--directory", type=Path)
    args = parser.parse_args()
    directory = args.directory or (ROOT / RELEASE if args.command == "verify"
                                  else ROOT / ".pio/build/cardputer-adv/release")
    manifest = verify(directory)
    if args.command == "publish":
        require(not manifest["source_dirty"], "Commit build inputs, then rebuild before publishing")
        (ROOT / RELEASE).mkdir(parents=True, exist_ok=True)
        for name in FILES:
            shutil.copyfile(directory / name, ROOT / RELEASE / name)
        print(f"Copied verified firmware to {RELEASE}; commit these files with Git")
    else:
        require(not manifest["source_dirty"], "Distribution binaries must be built from committed inputs")
        print(f"Verified {directory}: hashes, ESP32-S3 images, layout and source freshness")


if __name__ == "__main__":
    try:
        main()
    except (ValueError, KeyError, OSError) as error:
        sys.exit(str(error))
