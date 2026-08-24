"""Verify the executable bootstrap contract for deterministic asset tooling."""

from __future__ import annotations

from importlib import metadata
from pathlib import Path
import re
import shutil
import subprocess
import sys
import unittest


ROOT = Path(__file__).resolve().parents[2]
REQUIREMENTS = ROOT / "tools" / "requirements-assets.txt"
PIN = re.compile(r"([A-Za-z0-9_.-]+)==([^\s]+)")
REQUIRED_TOOLS = {
    "fonttools": "4.58.0",
    "pillow": "11.2.1",
}


def normalized(name: str) -> str:
    return re.sub(r"[-_.]+", "-", name).lower()


class AssetDependencyTest(unittest.TestCase):
    def test_required_asset_libraries_are_exactly_pinned_and_importable(self) -> None:
        pins: dict[str, str] = {}
        for line in REQUIREMENTS.read_text(encoding="utf-8").splitlines():
            line = line.strip()
            if not line or line.startswith("#"):
                continue
            match = PIN.fullmatch(line)
            self.assertIsNotNone(match, f"asset dependency is not exactly pinned: {line}")
            assert match is not None
            pins[normalized(match.group(1))] = match.group(2)

        self.assertTrue(REQUIRED_TOOLS.keys() <= pins.keys())
        for package, expected_version in REQUIRED_TOOLS.items():
            self.assertEqual(pins[package], expected_version)
            self.assertEqual(metadata.version(package), expected_version)

        for tool in ("tools/build_font.py", "tools/process_cats.py"):
            subprocess.run(
                [sys.executable, tool, "--help"],
                cwd=ROOT,
                check=True,
                text=True,
                capture_output=True,
            )

    def test_make_exposes_the_pinned_dependency_bootstrap(self) -> None:
        make = shutil.which("make")
        self.assertIsNotNone(make, "make is required for bootstrap contract tests")
        result = subprocess.run(
            [make, "--dry-run", f"PYTHON={sys.executable}", "asset-deps"],
            cwd=ROOT,
            check=True,
            text=True,
            capture_output=True,
        )
        self.assertIn("-m pip install", result.stdout)
        self.assertIn("tools/requirements-assets.txt", result.stdout)


if __name__ == "__main__":
    unittest.main()
