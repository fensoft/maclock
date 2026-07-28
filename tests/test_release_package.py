import importlib.util
from pathlib import Path
import tempfile
import unittest
import zipfile


ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location(
    "package_release", ROOT / "scripts" / "package_release.py"
)
package_release = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
SPEC.loader.exec_module(package_release)


class ReleasePackageTests(unittest.TestCase):
    def test_deterministic_zip_and_manifest(self):
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            first = directory / "first.zip"
            second = directory / "second.zip"
            source_a = directory / "a.bin"
            source_b = directory / "b.txt"
            source_a.write_bytes(bytes(range(256)) * 8)
            source_b.write_text("Maclock\n", encoding="utf-8")
            files = [
                (source_a, "images/a.bin"),
                (source_b, "b.txt"),
            ]

            package_release.build_assets_zip(first, files)
            package_release.build_assets_zip(second, files)

            self.assertEqual(first.read_bytes(), second.read_bytes())
            manifest = package_release.inspect_assets_zip(first)
            self.assertEqual(
                [item["path"] for item in manifest],
                ["/images/a.bin", "/b.txt"],
            )
            self.assertTrue(
                all(
                    item["method"] == zipfile.ZIP_DEFLATED
                    for item in manifest
                )
            )

    def test_downloaded_entry_is_rejected(self):
        with tempfile.TemporaryDirectory() as temporary:
            archive_path = Path(temporary) / "unsafe.zip"
            with zipfile.ZipFile(
                archive_path, "w", allowZip64=False
            ) as archive:
                archive.writestr("downloaded/user.mp3", b"audio")

            with self.assertRaisesRegex(ValueError, "Unsafe ZIP entry"):
                package_release.inspect_assets_zip(archive_path)

    def test_traversal_entry_is_rejected(self):
        with tempfile.TemporaryDirectory() as temporary:
            archive_path = Path(temporary) / "unsafe.zip"
            with zipfile.ZipFile(
                archive_path, "w", allowZip64=False
            ) as archive:
                archive.writestr("../escape.txt", b"escape")

            with self.assertRaisesRegex(ValueError, "Unsafe ZIP entry"):
                package_release.inspect_assets_zip(archive_path)

    def test_corrupt_zip_is_rejected(self):
        with tempfile.TemporaryDirectory() as temporary:
            archive_path = Path(temporary) / "corrupt.zip"
            archive_path.write_bytes(b"not a zip")
            with self.assertRaises(zipfile.BadZipFile):
                package_release.inspect_assets_zip(archive_path)


if __name__ == "__main__":
    unittest.main()
