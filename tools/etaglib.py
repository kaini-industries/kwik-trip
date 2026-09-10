"""Dependency-free profile validation and target-image packaging (Python 3.10+)."""
from __future__ import annotations

import hashlib
import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PROTOCOLS = {"unknown", "ti_cc_debug", "swd", "uart_bootloader", "spi_flash"}


def read_json(path: Path):
    def unique(pairs):
        result = {}
        for key, value in pairs:
            if key in result:
                raise ValueError(f"duplicate JSON key: {key}")
            result[key] = value
        return result
    return json.loads(path.read_text(), object_pairs_hook=unique)


def keys(value, expected, name):
    if not isinstance(value, dict) or set(value) != set(expected.split()):
        raise ValueError(f"{name}: expected keys: {expected}")


def integer(value, low, high, name, nullable=False):
    if nullable and value is None:
        return
    if type(value) is not int or not low <= value <= high:
        raise ValueError(f"{name}: expected integer {low}..{high}")


def text_value(value, name, nullable=False):
    if nullable and value is None:
        return
    if not isinstance(value, str) or not value.strip() or len(value) > 512:
        raise ValueError(f"{name}: expected nonempty text up to 512 characters")


def validate_profile(p, root=ROOT):
    keys(p, "schema_version id manufacturer model revision status protocol mcu power display evidence notes", "profile")
    integer(p["schema_version"], 1, 1, "schema_version")
    if not isinstance(p["id"], str) or not re.fullmatch(r"[a-z][a-z0-9-]{1,63}", p["id"]):
        raise ValueError("profile id must be a lowercase slug")
    for field in ("manufacturer", "model"):
        text_value(p[field], field)
    text_value(p["revision"], "revision", nullable=True)
    if not isinstance(p["status"], str) or not isinstance(p["protocol"], str) or p["status"] not in ("unknown", "candidate", "verified") or p["protocol"] not in PROTOCOLS:
        raise ValueError("unknown profile status or protocol")
    m = p["mcu"]
    keys(m, "family part chip_id flash_bytes", "mcu")
    for field in ("family", "part"):
        text_value(m[field], f"mcu.{field}", nullable=True)
    integer(m["chip_id"], 0, 0xFFFFFFFF, "chip_id", True)
    integer(m["flash_bytes"], 1, 16777216, "flash_bytes", True)
    keys(p["power"], "target_mv", "power")
    integer(p["power"]["target_mv"], 1000, 5000, "target_mv", True)
    d = p["display"]
    keys(d, "controller width height colors", "display")
    text_value(d["controller"], "controller", nullable=True)
    for field in ("width", "height"):
        integer(d[field], 1, 4096, field, True)
    if (d["width"] is None) != (d["height"] is None):
        raise ValueError("display dimensions must both be known or unknown")
    for field, values in (("colors", d["colors"]), ("notes", p["notes"]), ("evidence", p["evidence"])):
        if not isinstance(values, list):
            raise ValueError(f"{field}: expected list")
        for item in values:
            text_value(item, field)
    for evidence in p["evidence"]:
        path = (root / evidence).resolve()
        if Path(evidence).is_absolute() or not path.is_relative_to(root.resolve()) or not path.is_file():
            raise ValueError(f"missing or unsafe evidence path: {evidence}")
    if p["protocol"] == "ti_cc_debug":
        if m["family"] != "CC2510" or m["chip_id"] != 0x81:
            raise ValueError("current TI backend supports CC2510 model 0x81 only")
        if m["flash_bytes"] is not None and m["flash_bytes"] not in (8192, 16384, 32768):
            raise ValueError("invalid CC2510 capacity")
        if p["power"]["target_mv"] is not None and not 2000 <= p["power"]["target_mv"] <= 3600:
            raise ValueError("CC2510 target supply is outside 2.0..3.6 V")
    if p["status"] == "verified":
        if p["protocol"] == "unknown" or not p["revision"] or not p["evidence"]:
            raise ValueError("verified profile requires protocol, PCB revision and evidence")
        if any(m[k] is None for k in m) or p["power"]["target_mv"] is None:
            raise ValueError("verified profile requires complete MCU and power fields")
        if m["family"] == "CC2510" and m["part"] != f'CC2510F{m["flash_bytes"] // 1024}':
            raise ValueError("CC2510 part suffix and capacity disagree")
    return p


def load_profiles(root=ROOT):
    profiles = [validate_profile(read_json(path), root) for path in sorted((root / "profiles/tags").glob("*.json"))]
    ids = [p["id"] for p in profiles]
    if not profiles or len(ids) != len(set(ids)):
        raise ValueError("profile catalog must be nonempty with unique IDs")
    return profiles


def load_hosts(root=ROOT):
    catalog = read_json(root / "config/hosts.json")
    keys(catalog, "schema_version hosts", "hosts catalog")
    integer(catalog["schema_version"], 1, 1, "host schema_version")
    if not isinstance(catalog["hosts"], list) or not catalog["hosts"]:
        raise ValueError("hosts must be a nonempty list")
    ids = set()
    for h in catalog["hosts"]:
        keys(h, "id name board soc dd dc reset reserved_pins notes", "host")
        for field in ("id", "name", "board", "soc", "notes"):
            text_value(h[field], field)
        if not re.fullmatch(r"[a-z][a-z0-9-]{1,63}", h["id"]) or h["id"] in ids:
            raise ValueError("invalid or duplicate host id")
        ids.add(h["id"])
        valid = set(range(22)) | set(range(26, 49)) if h["soc"] == "esp32s3" else set(range(34)) - {20, 24, 28, 29, 30, 31}
        if h["soc"] not in ("esp32", "esp32s3"):
            raise ValueError("unknown host SoC")
        if not isinstance(h["reserved_pins"], list):
            raise ValueError("reserved_pins must be a list")
        for pin in h["reserved_pins"]:
            integer(pin, 0, 48, "reserved pin")
        pins = [h[k] for k in ("dd", "dc", "reset")]
        for pin in pins:
            integer(pin, 0, 48, "debug pin")
        forbidden = set(h["reserved_pins"])
        forbidden |= {0, 3, 19, 20, 26, 27, 28, 29, 30, 31, 32, 45, 46} if h["soc"] == "esp32s3" else {0, 1, 2, 3, 5, 6, 7, 8, 9, 10, 11, 12, 15}
        if len(set(pins)) != 3 or not set(pins) <= valid or set(pins) & forbidden:
            raise ValueError(f'{h["id"]}: invalid, duplicate or reserved GPIO assignment')
    return catalog["hosts"]


def generated_header(host, profiles):
    quote = lambda value: json.dumps(value, ensure_ascii=True)
    lines = ["// Generated from config/hosts.json and profiles/tags. Do not edit.",
             "#pragma once", "#include <etag/core.h>", "namespace etag_build {",
             f'constexpr const char* kHostId = {quote(host["id"])};',
             f'constexpr const char* kHostName = {quote(host["name"])};',
             f'constexpr int kDD = {host["dd"]}, kDC = {host["dc"]}, kReset = {host["reset"]};',
             "constexpr etag::TargetProfile kProfiles[] = {"]
    for p in profiles:
        protocol = "TiCcDebug" if p["protocol"] == "ti_cc_debug" else "Unknown"
        lines.append("    {%s, %s, etag::Protocol::%s, %s, %d, %d}," % (
            quote(p["id"]), quote(p["model"]), protocol,
            "true" if p["status"] == "verified" else "false",
            (p["mcu"]["chip_id"] or 0) if protocol == "TiCcDebug" else 0, p["mcu"]["flash_bytes"] or 0))
    lines += ["};", "constexpr size_t kProfileCount = sizeof(kProfiles) / sizeof(kProfiles[0]);", "}"]
    return "\n".join(lines) + "\n"


def parse_hex(text, capacity):
    """Strict Intel HEX decoder: retain origin, reject overlap, checksum/range errors."""
    image = bytearray(b"\xff" * capacity)
    seen = set()
    base = 0
    ended = False
    high = 0
    for number, line in enumerate(text.splitlines(), 1):
        line = line.strip()
        if not line:
            continue
        if ended or not re.fullmatch(r":[0-9a-fA-F]+", line):
            raise ValueError(f"HEX line {number}: invalid record or data after EOF")
        try:
            record = bytes.fromhex(line[1:])
        except ValueError as exc:
            raise ValueError(f"HEX line {number}: malformed bytes") from exc
        if len(record) < 5 or record[0] + 5 != len(record) or sum(record) % 256:
            raise ValueError(f"HEX line {number}: invalid length/checksum")
        size, address, kind = record[0], int.from_bytes(record[1:3], "big"), record[3]
        data = record[4:-1]
        if kind == 0:
            start = base + address
            if address + size > 65536 or start + size > capacity:
                raise ValueError("HEX data exceeds address range or target capacity")
            for index, value in enumerate(data, start):
                if index in seen:
                    raise ValueError("overlapping HEX records")
                seen.add(index)
                image[index] = value
            high = max(high, start + size)
        elif kind == 1 and size == 0 and address == 0:
            ended = True
        elif kind in (2, 4) and size == 2 and address == 0:
            base = int.from_bytes(data, "big") << (4 if kind == 2 else 16)
        else:
            raise ValueError(f"unsupported or malformed HEX record type {kind}")
    if not ended or 0 not in seen or not high:
        raise ValueError("HEX requires EOF and data at reset address zero")
    return bytes(image), high


def image_bytes(path, capacity):
    if path.suffix.lower() in (".hex", ".ihx"):
        return parse_hex(path.read_text(encoding="ascii"), capacity)
    if path.suffix.lower() != ".bin":
        raise ValueError("input must be .bin, .hex or .ihx")
    data = path.read_bytes()
    if not data or len(data) > capacity:
        raise ValueError("binary is empty or larger than target capacity")
    return data.ljust(capacity, b"\xff"), len(data)


def sha256(data):
    return hashlib.sha256(data).hexdigest()


def create_package(source, profile, output, root=ROOT):
    profile = validate_profile(profile, root)
    if profile["status"] != "verified":
        raise ValueError("packaging requires a physically verified target profile")
    data, actual = image_bytes(source, profile["mcu"]["flash_bytes"])
    if all(value == 255 for value in data):
        raise ValueError("refusing an all-erased image")
    manifest = {"schema_version": 1, "kind": "tag-firmware", "profile_id": profile["id"],
                "protocol": profile["protocol"], "mcu": profile["mcu"]["part"],
                "load_address": 0, "source_length": actual, "length": len(data),
                "sha256": sha256(data), "file": "firmware.bin", "profile": profile}
    output.mkdir(parents=True, exist_ok=False)  # Never overwrite an existing package.
    (output / "firmware.bin").write_bytes(data)
    (output / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
    return manifest


def verify_package(path, root=ROOT):
    m = read_json(path / "manifest.json")
    keys(m, "schema_version kind profile_id protocol mcu load_address source_length length sha256 file profile", "manifest")
    integer(m["schema_version"], 1, 1, "manifest schema_version")
    integer(m["load_address"], 0, 0, "load_address")
    if m["kind"] != "tag-firmware" or m["file"] != "firmware.bin" or m["load_address"] != 0:
        raise ValueError("invalid package type, file or load address")
    p = validate_profile(m["profile"], root)
    if p["status"] != "verified" or m["profile_id"] != p["id"] or m["protocol"] != p["protocol"] or m["mcu"] != p["mcu"]["part"]:
        raise ValueError("package target identity disagrees with profile")
    integer(m["length"], 1, 16777216, "length")
    integer(m["source_length"], 1, m["length"], "source_length")
    data = (path / "firmware.bin").read_bytes()
    if len(data) != m["length"] or len(data) != p["mcu"]["flash_bytes"] or sha256(data) != m["sha256"]:
        raise ValueError("firmware capacity, length or hash mismatch")
    if all(value == 255 for value in data):
        raise ValueError("all-erased firmware is not a valid package")
    if any(b != 255 for b in data[m["source_length"]:]):
        raise ValueError("non-erased bytes after source length")
    return m
