import copy
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
import etaglib as e


def record(address, kind, data=b""):
    payload = bytes([len(data), address >> 8, address & 255, kind]) + data
    return ":" + (payload + bytes([-sum(payload) & 255])).hex().upper()


class ProfileTests(unittest.TestCase):
    def setUp(self):
        self.p = copy.deepcopy(e.load_profiles()[0])

    def test_candidates_do_not_claim_capacity_or_verified_hardware(self):
        self.assertIsNone(self.p["mcu"]["flash_bytes"])
        self.assertEqual("candidate", self.p["status"])

    def test_boolean_is_not_an_integer_capacity(self):
        self.p["mcu"]["flash_bytes"] = True
        with self.assertRaises(ValueError): e.validate_profile(self.p)

    def test_verified_requires_physical_evidence_and_memory(self):
        self.p["status"] = "verified"
        with self.assertRaises(ValueError): e.validate_profile(self.p)

    def test_unknown_fields_are_rejected(self):
        self.p["flash_size"] = 32768
        with self.assertRaises(ValueError): e.validate_profile(self.p)

    def test_unsafe_or_missing_evidence_is_rejected(self):
        self.p["evidence"] = ["../../etc/passwd"]
        with self.assertRaises(ValueError): e.validate_profile(self.p)

    def test_chip_and_voltage_constraints(self):
        self.p["power"]["target_mv"] = 5000
        with self.assertRaises(ValueError): e.validate_profile(self.p)
        self.p["power"]["target_mv"] = None
        self.p["mcu"]["chip_id"] = 0x91
        with self.assertRaises(ValueError): e.validate_profile(self.p)

    def test_reserved_and_duplicate_host_pins_rejected(self):
        catalog = e.read_json(ROOT / "config/hosts.json")
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "config").mkdir()
            for pin in (3, 8, 19, 23, 26, 6):
                changed = copy.deepcopy(catalog)
                changed["hosts"][0]["dd"] = pin
                (root / "config/hosts.json").write_text(json.dumps(changed))
                with self.subTest(pin=pin), self.assertRaises(ValueError): e.load_hosts(root)

    def test_header_is_deterministic_and_unknown_protocol_stays_unknown(self):
        h = e.load_hosts()[0]
        profiles = e.load_profiles()
        first = e.generated_header(h, profiles)
        self.assertEqual(first, e.generated_header(h, profiles))
        self.assertIn("etag::Protocol::Unknown", first)
        self.assertIn("kDD = 4, kDC = 6, kReset = 15", first)


class HexTests(unittest.TestCase):
    def test_sparse_image_retains_origin_and_ff_gaps(self):
        text = "\n".join([record(0, 0, b"\x02\x00\x20"), record(32, 0, b"\xA5"), record(0, 1)])
        data, length = e.parse_hex(text, 64)
        self.assertEqual(33, length)
        self.assertEqual(b"\x02\x00\x20", data[:3])
        self.assertEqual(b"\xff" * 29, data[3:32])
        self.assertEqual(0xA5, data[32])

    def test_bad_checksum_overlap_and_trailing_data(self):
        good = record(0, 0, b"\x02")
        cases = [good[:-2] + "00\n" + record(0, 1), good + "\n" + good + "\n" + record(0, 1),
                 good + "\n" + record(0, 1) + "\n" + good]
        for text in cases:
            with self.subTest(text=text), self.assertRaises(ValueError): e.parse_hex(text, 64)

    def test_missing_origin_or_eof(self):
        for text in [record(0, 0, b"\x02"), record(4, 0, b"\x02") + "\n" + record(0, 1)]:
            with self.assertRaises(ValueError): e.parse_hex(text, 64)

    def test_extended_addresses_respect_capacity(self):
        for kind in (2, 4):
            text = "\n".join([record(0, kind, b"\x00\x01"), record(0, 0, b"x"), record(0, 1)])
            with self.assertRaises(ValueError): e.parse_hex(text, 8)

    def test_malformed_records_and_address_wrap(self):
        for text in [":abc", ":zzzz", record(1, 1), record(0, 3, b"1234"), record(65535, 0, b"12")]:
            with self.subTest(text=text), self.assertRaises(ValueError): e.parse_hex(text, 65536)


class PackageTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        (self.root / "evidence.txt").write_text("Synthetic test fixture, not a real verified tag.")
        self.p = copy.deepcopy(e.load_profiles()[0])
        self.p.update(status="verified", revision="synthetic", evidence=["evidence.txt"])
        self.p["mcu"].update(part="CC2510F32", flash_bytes=32768)
        self.p["power"]["target_mv"] = 3300
        self.source = self.root / "input.bin"
        self.source.write_bytes(b"\x02\x00\x10")

    def test_package_round_trip_padding_and_no_overwrite(self):
        output = self.root / "package"
        manifest = e.create_package(self.source, self.p, output, self.root)
        self.assertEqual(3, manifest["source_length"])
        self.assertEqual(32768, manifest["length"])
        self.assertEqual(manifest, e.verify_package(output, self.root))
        with self.assertRaises(FileExistsError): e.create_package(self.source, self.p, output, self.root)

    def test_unverified_and_oversized_images_rejected_before_output(self):
        self.p["status"] = "candidate"
        with self.assertRaises(ValueError): e.create_package(self.source, self.p, self.root / "bad", self.root)
        self.assertFalse((self.root / "bad").exists())
        self.p["status"] = "verified"
        self.source.write_bytes(b"x" * 32769)
        with self.assertRaises(ValueError): e.create_package(self.source, self.p, self.root / "bad", self.root)

    def test_corrupt_package_is_rejected(self):
        output = self.root / "package"
        e.create_package(self.source, self.p, output, self.root)
        with (output / "firmware.bin").open("r+b") as stream: stream.write(b"bad")
        with self.assertRaises(ValueError): e.verify_package(output, self.root)

    def test_manifest_identity_and_filename_cannot_be_substituted(self):
        output = self.root / "package"
        manifest = e.create_package(self.source, self.p, output, self.root)
        for key, value in [("file", "../input.bin"), ("profile_id", "different"), ("length", 3), ("source_length", 0)]:
            changed = dict(manifest)
            changed[key] = value
            (output / "manifest.json").write_text(json.dumps(changed))
            with self.subTest(key=key), self.assertRaises(ValueError): e.verify_package(output, self.root)

    def test_part_and_capacity_must_agree(self):
        self.p["mcu"]["part"] = "CC2510F8"
        with self.assertRaises(ValueError): e.validate_profile(self.p, self.root)

    def test_duplicate_json_keys_fail(self):
        path = self.root / "duplicate.json"
        path.write_text('{"id":1,"id":2}')
        with self.assertRaises(ValueError): e.read_json(path)

    def test_backup_rejects_identical_input_path(self):
        p = copy.deepcopy(self.p)
        p["evidence"] = e.load_profiles()[0]["evidence"]
        path = self.root / "profile.json"
        path.write_text(json.dumps(p))
        result = subprocess.run([sys.executable, str(ROOT / "tools/etag.py"), "backup",
                                 "--first", str(self.source), "--second", str(self.source),
                                 "--profile", str(path), "--unit", "test", "--output", str(self.root)],
                                capture_output=True, text=True)
        self.assertEqual(2, result.returncode)
        self.assertIn("separately captured", result.stderr)

    def run_backup(self, first, second):
        profile = copy.deepcopy(self.p)
        profile["evidence"] = e.load_profiles()[0]["evidence"]
        path = self.root / "profile.json"
        path.write_text(json.dumps(profile))
        a, b = self.root / "first.bin", self.root / "second.bin"
        a.write_bytes(first)
        b.write_bytes(second)
        return subprocess.run(
            [sys.executable, str(ROOT / "tools/etag.py"), "backup",
             "--first", str(a), "--second", str(b), "--profile", str(path),
             "--unit", "synthetic-unit", "--output", str(self.root / "backups")],
            capture_output=True, text=True)

    def test_backup_preserves_bytes_and_never_overwrites_unit(self):
        data = bytes(range(256)) * 128
        self.assertEqual(0, self.run_backup(data, data).returncode)
        output = self.root / "backups/synthetic-unit"
        self.assertEqual(data, (output / "main.bin").read_bytes())
        metadata = e.read_json(output / "metadata.json")
        self.assertEqual(e.sha256(data), metadata["sha256"])
        self.assertEqual(2, metadata["matching_reads"])
        self.assertFalse(metadata["complete_device_backup"])
        self.assertEqual(2, self.run_backup(b"x" * 32768, b"x" * 32768).returncode)
        self.assertEqual(data, (output / "main.bin").read_bytes())

    def test_bad_backups_leave_no_archive(self):
        cases = [(b"short", b"short"), (b"x" * 32768, b"y" * 32768),
                 (bytes(32768), bytes(32768)), (b"\xff" * 32768, b"\xff" * 32768)]
        for first, second in cases:
            with self.subTest(length=len(first), first_byte=first[0]):
                self.assertEqual(2, self.run_backup(first, second).returncode)
                self.assertFalse((self.root / "backups/synthetic-unit").exists())


if __name__ == "__main__":
    unittest.main()
