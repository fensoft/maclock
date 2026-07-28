#!/usr/bin/env python3
"""Build deterministic Maclock firmware and LittleFS OTA release artifacts."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import stat
import struct
import subprocess
import sys
import zipfile


BOARD_ID = "lolin_s3"
PARTITION_SCHEMA = 2
ASSET_SCHEMA = 1
MAX_FIRMWARE_SIZE = 3 * 1024 * 1024
FIRMWARE_NAME = "maclock-lolin-s3-firmware.bin"
MANIFEST_NAME = "maclock-lolin-s3-update.json"
ASSETS_NAME = "maclock-lolin-s3-assets.zip"
CHECKSUMS_NAME = "SHA256SUMS.txt"
ZIP_TIMESTAMP = (1980, 1, 1, 0, 0, 0)


def sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def tracked_data_files(root: Path) -> list[tuple[Path, str]]:
    result = subprocess.run(
        ["git", "ls-files", "-z", "--", "data"],
        cwd=root,
        check=True,
        capture_output=True,
    )
    files: list[tuple[Path, str]] = []
    for raw_name in result.stdout.split(b"\0"):
        if not raw_name:
            continue
        repository_name = raw_name.decode("utf-8")
        relative = PurePosixPath(repository_name)
        if relative.parts[:1] != ("data",) or len(relative.parts) < 2:
            raise ValueError(f"Unexpected tracked data path: {repository_name}")
        archive_name = PurePosixPath(*relative.parts[1:]).as_posix()
        if (
            archive_name == "downloaded"
            or archive_name.startswith("downloaded/")
        ):
            raise ValueError(
                "Tracked release assets must never contain data/downloaded/"
            )
        normalized = PurePosixPath(archive_name)
        if (
            normalized.is_absolute()
            or ".." in normalized.parts
            or "\\" in archive_name
        ):
            raise ValueError(f"Unsafe tracked asset path: {repository_name}")
        source = root / repository_name
        if source.is_symlink():
            raise ValueError(f"Release assets may not be symlinks: {repository_name}")
        if not source.is_file():
            raise ValueError(f"Tracked asset is not a regular file: {repository_name}")
        files.append((source, archive_name))
    files.sort(key=lambda item: item[1].encode("utf-8"))
    if not files:
        raise ValueError("No tracked files were found below data/")
    return files


def build_assets_zip(
    destination: Path, files: list[tuple[Path, str]]
) -> None:
    destination.parent.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(
        destination,
        "w",
        compression=zipfile.ZIP_DEFLATED,
        compresslevel=9,
        allowZip64=False,
    ) as archive:
        for source, archive_name in files:
            info = zipfile.ZipInfo(archive_name, ZIP_TIMESTAMP)
            info.compress_type = zipfile.ZIP_DEFLATED
            info.create_system = 3
            info.external_attr = (stat.S_IFREG | 0o644) << 16
            info.flag_bits = 0
            archive.writestr(info, source.read_bytes(), compresslevel=9)


def inspect_assets_zip(path: Path) -> list[dict[str, object]]:
    blob = path.read_bytes()
    manifest_files: list[dict[str, object]] = []
    with zipfile.ZipFile(path, "r", allowZip64=False) as archive:
        for info in archive.infolist():
            name = info.filename
            normalized = PurePosixPath(name)
            if (
                info.is_dir()
                or normalized.is_absolute()
                or ".." in normalized.parts
                or "\\" in name
                or name == "downloaded"
                or name.startswith("downloaded/")
            ):
                raise ValueError(f"Unsafe ZIP entry: {name}")
            if info.compress_type not in (
                zipfile.ZIP_STORED,
                zipfile.ZIP_DEFLATED,
            ):
                raise ValueError(f"Unsupported ZIP method for {name}")
            if info.flag_bits & 0x0009:
                raise ValueError(f"Encrypted/data-descriptor ZIP entry: {name}")
            if info.file_size > 0xFFFFFFFF or info.compress_size > 0xFFFFFFFF:
                raise ValueError(f"ZIP64 entry is not supported: {name}")
            mode = (info.external_attr >> 16) & 0xFFFF
            if mode and not stat.S_ISREG(mode):
                raise ValueError(f"Non-regular ZIP entry: {name}")

            offset = info.header_offset
            if blob[offset : offset + 4] != b"PK\x03\x04":
                raise ValueError(f"Missing local ZIP header for {name}")
            (
                _version,
                flags,
                method,
                _time,
                _date,
                _crc,
                compressed_size,
                uncompressed_size,
                name_length,
                extra_length,
            ) = struct.unpack_from("<HHHHHIIIHH", blob, offset + 4)
            local_name = blob[
                offset + 30 : offset + 30 + name_length
            ].decode("utf-8")
            if (
                flags & 0x0009
                or local_name != name
                or method != info.compress_type
                or compressed_size != info.compress_size
                or uncompressed_size != info.file_size
                or extra_length > 4096
            ):
                raise ValueError(f"Unsupported local ZIP header for {name}")

            data = archive.read(info)
            if len(data) != info.file_size:
                raise ValueError(f"ZIP entry size mismatch for {name}")
            manifest_files.append(
                {
                    "path": f"/{name}",
                    "size": info.file_size,
                    "compressedSize": info.compress_size,
                    "method": info.compress_type,
                    "sha256": sha256_bytes(data),
                }
            )
    return manifest_files


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--version", default="1.0.0")
    parser.add_argument(
        "--firmware",
        type=Path,
        default=Path(".pio/build/lolin_s3/firmware.bin"),
    )
    parser.add_argument(
        "--output",
        type=Path,
        default=Path("dist/release/v1.0.0"),
    )
    return parser.parse_args()


def main() -> int:
    args = parse_arguments()
    root = Path(__file__).resolve().parent.parent
    firmware_source = (
        args.firmware
        if args.firmware.is_absolute()
        else root / args.firmware
    )
    output = args.output if args.output.is_absolute() else root / args.output

    if not firmware_source.is_file():
        raise FileNotFoundError(f"Firmware not found: {firmware_source}")
    firmware_size = firmware_source.stat().st_size
    if firmware_size <= 0 or firmware_size > MAX_FIRMWARE_SIZE:
        raise ValueError(
            f"Firmware is {firmware_size} bytes; the app slot limit is "
            f"{MAX_FIRMWARE_SIZE} bytes"
        )

    version = args.version.removeprefix("v")
    if (
        len(version.split(".")) != 3
        or not all(part.isdigit() for part in version.split("."))
    ):
        raise ValueError(f"Version must be semantic major.minor.patch: {version}")

    output.mkdir(parents=True, exist_ok=True)
    firmware_path = output / FIRMWARE_NAME
    firmware_path.write_bytes(firmware_source.read_bytes())

    assets_path = output / ASSETS_NAME
    files = tracked_data_files(root)
    build_assets_zip(assets_path, files)
    manifest_files = inspect_assets_zip(assets_path)
    if len(manifest_files) != len(files):
        raise ValueError("ZIP contents differ from tracked release assets")

    manifest = {
        "schema": 1,
        "version": version,
        "board": BOARD_ID,
        "partitionSchema": PARTITION_SCHEMA,
        "assetSchema": ASSET_SCHEMA,
        "firmware": {
            "name": FIRMWARE_NAME,
            "size": firmware_path.stat().st_size,
            "sha256": sha256_file(firmware_path),
        },
        "assets": {
            "name": ASSETS_NAME,
            "size": assets_path.stat().st_size,
            "sha256": sha256_file(assets_path),
            "files": manifest_files,
        },
    }
    manifest_path = output / MANIFEST_NAME
    manifest_path.write_text(
        json.dumps(manifest, indent=2, ensure_ascii=False) + "\n",
        encoding="utf-8",
        newline="\n",
    )

    artifacts = [firmware_path, manifest_path, assets_path]
    checksum_path = output / CHECKSUMS_NAME
    checksum_path.write_text(
        "".join(
            f"{sha256_file(path)}  {path.name}\n" for path in artifacts
        ),
        encoding="ascii",
        newline="\n",
    )
    print(
        f"Packaged Maclock {version}: {len(files)} tracked assets, "
        f"{firmware_size} firmware bytes"
    )
    for path in [*artifacts, checksum_path]:
        print(path)
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        print(f"release packaging failed: {error}", file=sys.stderr)
        sys.exit(1)
