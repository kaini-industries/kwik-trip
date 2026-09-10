#!/usr/bin/env python3
"""Local tooling; never opens a programmer or writes a target device."""
import argparse
from datetime import datetime, timezone
import json
from pathlib import Path
import re
import shutil
import sys
from urllib.parse import unquote

from etaglib import ROOT, create_package, load_hosts, load_profiles, read_json, sha256, validate_profile, verify_package


def check():
    profiles = load_profiles()
    hosts = load_hosts()
    manifest = read_json(ROOT / "docs/reference/migration-manifest.json")
    for record in manifest["files"]:
        if sha256((ROOT / record["path"]).read_bytes()) != record["sha256"]:
            raise ValueError(f'Original reference changed: {record["path"]}')
    for doc in ROOT.rglob("*.md"):
        relative = doc.relative_to(ROOT)
        if (any(part.startswith(".") for part in relative.parts) or
                relative.parts[0] == "artifacts" or
                str(relative).startswith("docs/reference/legacy/")):
            continue
        for target in re.findall(r"\]\(([^)]+)\)", doc.read_text()):
            target = target.split("#", 1)[0].strip("<>")
            if not target or "://" in target or target.startswith("mailto:"):
                continue
            if not (doc.parent / unquote(target)).exists():
                raise ValueError(f"Broken local link in {relative}: {target}")
    print(f"Validated {len(hosts)} hosts, {len(profiles)} tag profiles, original-file hashes, and documentation links.")


def backup(args):
    p = validate_profile(read_json(args.profile))
    capacity = p["mcu"]["flash_bytes"]
    if capacity is None:
        raise ValueError("Record measured MCU capacity in the profile before archiving a main-flash backup")
    if args.first.resolve() == args.second.resolve():
        raise ValueError("Provide two separately captured dump files")
    first, second = args.first.read_bytes(), args.second.read_bytes()
    if len(first) != capacity or first != second:
        raise ValueError("Dumps disagree or do not match the recorded capacity")
    if not re.fullmatch(r"[a-zA-Z0-9][a-zA-Z0-9_-]{0,63}", args.unit):
        raise ValueError("Unit must be a simple identifier, not a path")
    if not first or all(b == 255 for b in first) or all(b == 0 for b in first):
        raise ValueError("Uniform dump is not accepted as an original firmware backup")
    destination = args.output / args.unit
    destination.mkdir(parents=True, exist_ok=False)
    (destination / "main.bin").write_bytes(first)
    metadata = {"schema_version": 1, "kind": "main-flash-backup", "unit": args.unit,
                "profile": p, "captured_at": datetime.now(timezone.utc).isoformat(),
                "length": len(first), "sha256": sha256(first),
                "matching_reads": 2, "complete_device_backup": False,
                "notes": "Main flash only; information page, external flash and NFC are separate acquisitions."}
    (destination / "metadata.json").write_text(json.dumps(metadata, indent=2) + "\n")
    print(destination)


def new_profile(args):
    p = {"schema_version": 1, "id": args.id, "manufacturer": "Unknown", "model": args.id,
         "revision": None, "status": "unknown", "protocol": "unknown",
         "mcu": {"family": None, "part": None, "chip_id": None, "flash_bytes": None},
         "power": {"target_mv": None},
         "display": {"controller": None, "width": None, "height": None, "colors": []},
         "evidence": [], "notes": ["Identify hardware and add measured evidence before marking verified."]}
    validate_profile(p)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open("x") as stream:
        stream.write(json.dumps(p, indent=2) + "\n")
    print(args.output)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)
    commands.add_parser("check", help="Validate profiles, GPIO maps, reference hashes and local doc links")
    commands.add_parser("profiles", help="List known tags and confidence")
    commands.add_parser("doctor", help="Print tool availability without connecting to hardware")
    package = commands.add_parser("package", help="Create a new full-capacity tag firmware package")
    package.add_argument("source", type=Path)
    package.add_argument("--profile", type=Path, required=True)
    package.add_argument("--output", type=Path, required=True)
    verify = commands.add_parser("verify", help="Verify package structure, identity, length and SHA-256")
    verify.add_argument("package", type=Path)
    record = commands.add_parser("backup", help="Archive two matching, separately captured main-flash dumps")
    record.add_argument("--first", type=Path, required=True)
    record.add_argument("--second", type=Path, required=True)
    record.add_argument("--profile", type=Path, required=True)
    record.add_argument("--unit", required=True)
    record.add_argument("--output", type=Path, default=ROOT / "backups")
    new = commands.add_parser("new-profile", help="Create an unknown hardware profile without overwriting")
    new.add_argument("id")
    new.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    try:
        if args.command == "check":
            check()
        elif args.command == "profiles":
            for p in load_profiles():
                print(f'{p["id"]:24} {p["status"]:10} {p["protocol"]:16} {p["mcu"]["flash_bytes"] or "unknown"} bytes')
        elif args.command == "doctor":
            print(f"Python: {sys.version.split()[0]}\nPlatformIO: {shutil.which('pio') or 'not found'}")
            print("Run python -m pip install -r requirements-dev.txt inside a virtual environment if needed.")
        elif args.command == "package":
            m = create_package(args.source, read_json(args.profile), args.output)
            print(f'{args.output}: {m["length"]} bytes, SHA-256 {m["sha256"]}')
        elif args.command == "verify":
            m = verify_package(args.package)
            print(f'OK: {m["profile_id"]}, {m["length"]} bytes, {m["sha256"]}')
        elif args.command == "backup":
            backup(args)
        elif args.command == "new-profile":
            new_profile(args)
    except (ValueError, OSError) as exc:
        parser.exit(2, f"error: {exc}\n")


if __name__ == "__main__":
    main()
