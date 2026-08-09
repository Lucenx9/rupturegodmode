#!/usr/bin/env python3
"""Build the public release assets consumed by StarRupture ModLoader."""

import argparse
import json
from pathlib import Path
import re
import shutil
import zipfile


PLUGIN_NAME = "RuptureGodMode"
REPOSITORY = "Lucenx9/rupturegodmode"
INTERFACE_VERSION = 60
VERSION_PATTERN = re.compile(r"^v\d+\.\d+\.\d+$")


def json_bytes(value: object) -> bytes:
    return (json.dumps(value, indent=2) + "\n").encode("utf-8")


def write_zip(zip_path: Path, files: dict[str, bytes]) -> None:
    with zipfile.ZipFile(zip_path, "w", compression=zipfile.ZIP_DEFLATED) as package:
        for archive_name, contents in files.items():
            package.writestr(archive_name, contents)


def package_release(version: str, dll_path: Path, output_dir: Path) -> None:
    if not VERSION_PATTERN.fullmatch(version):
        raise ValueError("version must use the vMAJOR.MINOR.PATCH format")
    dll = dll_path.read_bytes()
    if not dll.startswith(b"MZ"):
        raise ValueError(f"plugin is not a Windows DLL: {dll_path}")

    project_root = Path(__file__).resolve().parents[1]
    installer = (project_root / "windows-package" / "Install-RuptureGodMode.ps1").read_bytes()
    windows_readme = (project_root / "windows-package" / "README-WINDOWS.txt").read_bytes()
    output_dir.mkdir(parents=True, exist_ok=True)

    manifest_name = f"{PLUGIN_NAME}-client-manifest.json"
    manifest = {
        "plugin_name": PLUGIN_NAME,
        "version": version,
        "interface_version_min": INTERFACE_VERSION,
        "interface_version_max": INTERFACE_VERSION,
        "download_url": (
            f"https://github.com/{REPOSITORY}/releases/download/{version}/"
            f"{PLUGIN_NAME}.dll"
        ),
    }
    expected_sidecar = {
        "manifest_url": (
            f"https://github.com/{REPOSITORY}/releases/latest/download/{manifest_name}"
        )
    }
    manifest_contents = json_bytes(manifest)
    sidecar_path = project_root / "release" / f"{PLUGIN_NAME}.json"
    sidecar_contents = sidecar_path.read_bytes()
    if json.loads(sidecar_contents) != expected_sidecar:
        raise ValueError(f"release sidecar has an unexpected manifest URL: {sidecar_path}")

    (output_dir / f"{PLUGIN_NAME}.dll").write_bytes(dll)
    (output_dir / manifest_name).write_bytes(manifest_contents)
    write_zip(
        output_dir / f"{PLUGIN_NAME}-Client-{version}.zip",
        {
            f"Plugins/{PLUGIN_NAME}.dll": dll,
            f"Plugins/{PLUGIN_NAME}.json": sidecar_contents,
        },
    )

    package_root = f"{PLUGIN_NAME}-Windows-{version}"
    write_zip(
        output_dir / f"{package_root}.zip",
        {
            f"{package_root}/Install-{PLUGIN_NAME}.ps1": installer,
            f"{package_root}/README-WINDOWS.txt": windows_readme,
            f"{package_root}/payload/{PLUGIN_NAME}.dll": dll,
            f"{package_root}/payload/{PLUGIN_NAME}.json": sidecar_contents,
        },
    )


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--version", required=True)
    parser.add_argument("--dll", required=True, type=Path)
    parser.add_argument("--output-dir", required=True, type=Path)
    arguments = parser.parse_args()
    package_release(arguments.version, arguments.dll, arguments.output_dir)


if __name__ == "__main__":
    main()
