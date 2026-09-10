"""Exercise the actual distributed images, including failure cases that block shipping."""
import copy
import json
from pathlib import Path
import shutil
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
import cardputer_release as release


class CardputerReleaseTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.directory = ROOT / release.RELEASE
        cls.factory = (cls.directory / release.FACTORY).read_bytes()
        cls.app = (cls.directory / release.APP).read_bytes()
        cls.manifest = json.loads((cls.directory / "manifest.json").read_text())

    def test_committed_bundle_is_current_and_valid(self):
        release.verify(self.directory)
        self.assertFalse(self.manifest["source_dirty"])

    def test_application_bit_flip_fails_esp_checksum(self):
        app = bytearray(self.app)
        app[100] ^= 1
        with self.assertRaisesRegex(ValueError, "checksum mismatch"):
            release.validate_esp_image(bytes(app))

    def test_corrupted_app_digest_is_rejected(self):
        app = bytearray(self.app)
        app[-1] ^= 1
        with self.assertRaisesRegex(ValueError, "SHA256 mismatch"):
            release.validate_esp_image(bytes(app))

    def test_app_cannot_be_used_as_factory(self):
        with self.assertRaises(ValueError):
            release.validate_factory(self.app, self.app, self.manifest["components"])

    def test_truncated_factory_is_rejected(self):
        with self.assertRaises(ValueError):
            release.validate_factory(self.factory[:-1], self.app, self.manifest["components"])

    def test_wrong_flash_offset_is_rejected(self):
        components = copy.deepcopy(self.manifest["components"])
        components[0]["offset"] = 0x1000  # ESP32 classic offset is wrong for the S3.
        with self.assertRaisesRegex(ValueError, "offsets"):
            release.validate_factory(self.factory, self.app, components)

    def test_factory_nvs_cannot_contain_saved_settings(self):
        factory = bytearray(self.factory)
        factory[0x9000] = 0
        with self.assertRaisesRegex(ValueError, "padding"):
            release.validate_factory(bytes(factory), self.app, self.manifest["components"])

    def test_bad_partition_md5_is_rejected_even_with_updated_file_hash(self):
        factory = bytearray(self.factory)
        factory[0x8000 + 12] ^= 1
        components = copy.deepcopy(self.manifest["components"])
        c = components[1]
        c["sha256"] = release.sha(factory[c["offset"]:c["offset"] + c["size"]])
        with self.assertRaisesRegex(ValueError, "Partition MD5"):
            release.validate_factory(bytes(factory), self.app, components)

    def test_checksum_file_corruption_is_rejected(self):
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            for name in release.FILES:
                shutil.copyfile(self.directory / name, directory / name)
            (directory / "SHA256SUMS").write_text("incorrect\n")
            with self.assertRaisesRegex(ValueError, "SHA256SUMS"):
                release.verify(directory)

    def test_firmware_edits_and_new_sources_make_bundle_stale(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            for name in release.source_inputs(ROOT):
                path = root / name
                path.parent.mkdir(parents=True, exist_ok=True)
                shutil.copyfile(ROOT / name, path)
            release.verify(self.directory, root)
            source = root / "firmware/programmer/src/main.cpp"
            original = source.read_bytes()
            source.write_bytes(original + b"\n// modified build input\n")
            with self.assertRaisesRegex(ValueError, "stale"):
                release.verify(self.directory, root)
            source.write_bytes(original)
            (source.parent / "new_driver.cpp").write_text("// new build input\n")
            with self.assertRaisesRegex(ValueError, "stale"):
                release.verify(self.directory, root)


if __name__ == "__main__":
    unittest.main()
