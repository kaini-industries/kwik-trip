"""Exercise the actual distributed images, including failure cases that block shipping."""
import copy
import importlib.metadata
import json
from pathlib import Path
import shutil
import subprocess
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

    def test_release_validation_uses_pinned_esptool_v5(self):
        self.assertEqual(importlib.metadata.version("esptool"), release.ESPTOOL_VERSION)
        release.validate_esp_image(self.app)

    def test_pinned_esptool_merges_factory_image(self):
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            images = []
            for index, component in enumerate(self.manifest["components"]):
                path = directory / f"component-{index}.bin"
                start = component["offset"]
                path.write_bytes(self.factory[start:start + component["size"]])
                images.append((start, path))
            output = directory / release.FACTORY
            release.merge_factory(output, images, release.SETTINGS)
            merged = output.read_bytes()
            components = copy.deepcopy(self.manifest["components"])
            for component in components:
                start = component["offset"]
                component["sha256"] = release.sha(
                    merged[start:start + component["size"]]
                )
            release.validate_factory(merged, self.app, components)
            self.assertEqual(merged, self.factory)

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

    def test_release_metadata_requires_canonical_utf8_lf(self):
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            for name in release.FILES:
                shutil.copyfile(self.directory / name, directory / name)
            manifest = (directory / "manifest.json").read_bytes().replace(b"\n", b"\r\n")
            (directory / "manifest.json").write_bytes(manifest)
            sums = "".join(
                f"{release.sha((directory / name).read_bytes())}  {name}\n"
                for name in (release.FACTORY, release.APP, "manifest.json")
            )
            release.write_utf8_lf(directory / "SHA256SUMS", sums)
            with self.assertRaisesRegex(ValueError, "UTF-8 without BOM and LF"):
                release.verify(directory)

    def test_metadata_writer_emits_exact_utf8_lf_bytes(self):
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / "metadata.json"
            release.write_utf8_lf(path, "{\"label\": \"Café\"}\n")
            self.assertEqual(path.read_bytes(), b'{"label": "Caf\xc3\xa9"}\n')
            with self.assertRaisesRegex(ValueError, "non-canonical"):
                release.write_utf8_lf(path, "{}\r\n")

    def test_git_checkout_preserves_release_metadata_lf(self):
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            shutil.copyfile(ROOT / ".gitattributes", directory / ".gitattributes")
            (directory / "manifest.json").write_bytes(b'{"schema_version": 1}\n')
            (directory / "SHA256SUMS").write_bytes(b"abc  manifest.json\n")
            commands = [
                ["git", "init", "-q"],
                ["git", "config", "user.email", "release-test@example.invalid"],
                ["git", "config", "user.name", "Release Test"],
                ["git", "config", "core.autocrlf", "true"],
                ["git", "add", ".gitattributes", "manifest.json", "SHA256SUMS"],
                ["git", "commit", "-qm", "fixture"],
            ]
            for command in commands:
                subprocess.run(command, cwd=directory, check=True, capture_output=True)
            for name in ("manifest.json", "SHA256SUMS"):
                (directory / name).unlink()
            subprocess.run(["git", "checkout", "--", "manifest.json", "SHA256SUMS"],
                           cwd=directory, check=True, capture_output=True)
            self.assertNotIn(b"\r", (directory / "manifest.json").read_bytes())
            self.assertNotIn(b"\r", (directory / "SHA256SUMS").read_bytes())

    def test_source_dirty_check_covers_deletions_and_each_untracked_root(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            for name in release.SOURCE_FILES:
                path = root / name
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text(f"fixture for {name}\n")

            extensions = (".cpp", ".hpp", ".cpp", ".json", ".ini")
            tracked_sources = []
            for directory_name, extension in zip(release.SOURCE_DIRECTORIES, extensions):
                path = root / directory_name / f"tracked_source{extension}"
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text("tracked build input\n")
                tracked_sources.append(path)

            ignored_document = root / "lib" / "NOTES.md"
            ignored_local_config = root / "config" / "platformio" / "local.ini"
            outside_document = root / "README.md"
            for path in (ignored_document, ignored_local_config, outside_document):
                path.write_text("ignored baseline\n")
            (root / ".gitignore").write_text("untracked_source*\n")

            commands = [
                ["git", "init", "-q"],
                ["git", "config", "user.email", "release-test@example.invalid"],
                ["git", "config", "user.name", "Release Test"],
                ["git", "add", "-f", "."],
                ["git", "commit", "-qm", "fixture"],
            ]
            for command in commands:
                subprocess.run(command, cwd=root, check=True, capture_output=True)
            self.assertEqual(release.changed_source_inputs(root), ())

            ignored_document.write_text("documentation-only edit\n")
            ignored_local_config.write_text("private local override\n")
            outside_document.write_text("outside source roots\n")
            self.assertEqual(release.changed_source_inputs(root), ())

            for path in (root / release.SOURCE_FILES[0], *tracked_sources):
                relative = path.relative_to(root).as_posix()
                path.unlink()
                self.assertEqual(release.changed_source_inputs(root), (relative,))
                subprocess.run(["git", "checkout", "--", relative], cwd=root, check=True,
                               capture_output=True)

            for directory_name, extension in zip(release.SOURCE_DIRECTORIES, extensions):
                path = root / directory_name / f"untracked_source{extension}"
                path.write_text("untracked build input\n")
                relative = path.relative_to(root).as_posix()
                self.assertEqual(release.changed_source_inputs(root), (relative,))
                path.unlink()

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
