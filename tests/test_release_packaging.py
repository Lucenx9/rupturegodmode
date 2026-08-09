import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest
import zipfile


ROOT = Path(__file__).resolve().parents[1]
PACKAGER = ROOT / "scripts" / "package_release.py"
INSTALLER = ROOT / "windows-package" / "Install-RuptureGodMode.ps1"
WINDOWS_README = ROOT / "windows-package" / "README-WINDOWS.txt"
RELEASE_WORKFLOW = ROOT / ".github" / "workflows" / "release.yml"
SIDECAR = ROOT / "release" / "RuptureGodMode.json"
PYTHON = shutil.which("python3") or sys.executable


class ReleasePackagingTest(unittest.TestCase):
    def test_semver_tags_publish_packaged_release_assets(self):
        workflow = RELEASE_WORKFLOW.read_text(encoding="utf-8")

        self.assertIn('tags: ["v*.*.*"]', workflow)
        self.assertIn("scripts/package_release.py", workflow)
        self.assertIn("release-assets/RuptureGodMode.dll", workflow)
        self.assertIn("release-assets/RuptureGodMode-client-manifest.json", workflow)
        self.assertIn("release-assets/RuptureGodMode-Client-$tag.zip", workflow)
        self.assertIn("release-assets/RuptureGodMode-Windows-$tag.zip", workflow)
        self.assertIn("gh release upload", workflow)
        self.assertIn("must be newer than", workflow)
        self.assertIn("is already published and immutable", workflow)
        self.assertIn("<!-- source-sha:$env:GITHUB_SHA -->", workflow)
        self.assertLess(
            workflow.index("- name: Run release tests"),
            workflow.index("- name: Publish complete release"),
        )

    def test_release_contains_modloader_metadata_and_windows_installer(self):
        with tempfile.TemporaryDirectory() as temporary_directory:
            temporary = Path(temporary_directory)
            plugin = temporary / "input.dll"
            plugin.write_bytes(b"MZtest-plugin")
            output = temporary / "release"

            subprocess.run(
                [
                    PYTHON,
                    str(PACKAGER),
                    "--version",
                    "v1.2.3",
                    "--dll",
                    str(plugin),
                    "--output-dir",
                    str(output),
                ],
                check=True,
            )

            manifest = json.loads(
                (output / "RuptureGodMode-client-manifest.json").read_text(
                    encoding="utf-8"
                )
            )
            self.assertEqual(
                manifest,
                {
                    "plugin_name": "RuptureGodMode",
                    "version": "v1.2.3",
                    "interface_version_min": 60,
                    "interface_version_max": 60,
                    "download_url": (
                        "https://github.com/Lucenx9/rupturegodmode/releases/"
                        "download/v1.2.3/RuptureGodMode.dll"
                    ),
                },
            )

            expected_sidecar = {
                "manifest_url": (
                    "https://github.com/Lucenx9/rupturegodmode/releases/"
                    "latest/download/RuptureGodMode-client-manifest.json"
                )
            }
            self.assertEqual(
                json.loads(SIDECAR.read_text(encoding="utf-8")), expected_sidecar
            )
            with zipfile.ZipFile(
                output / "RuptureGodMode-Client-v1.2.3.zip"
            ) as package:
                self.assertEqual(
                    package.read("Plugins/RuptureGodMode.dll"), plugin.read_bytes()
                )
                self.assertEqual(
                    json.loads(package.read("Plugins/RuptureGodMode.json")),
                    expected_sidecar,
                )

            package_root = "RuptureGodMode-Windows-v1.2.3"
            with zipfile.ZipFile(
                output / "RuptureGodMode-Windows-v1.2.3.zip"
            ) as package:
                self.assertEqual(
                    package.read(f"{package_root}/payload/RuptureGodMode.dll"),
                    plugin.read_bytes(),
                )
                self.assertEqual(
                    json.loads(
                        package.read(
                            f"{package_root}/payload/RuptureGodMode.json"
                        )
                    ),
                    expected_sidecar,
                )
                self.assertEqual(
                    package.read(f"{package_root}/Install-RuptureGodMode.ps1"),
                    INSTALLER.read_bytes(),
                )
                self.assertEqual(
                    package.read(f"{package_root}/README-WINDOWS.txt"),
                    WINDOWS_README.read_bytes(),
                )


if __name__ == "__main__":
    unittest.main()
