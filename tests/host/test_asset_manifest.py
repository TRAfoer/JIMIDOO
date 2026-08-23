"""Validate the deterministic cat-asset manifest and generated runtime artifacts."""

from __future__ import annotations

import json
from pathlib import Path
import unittest

from PIL import Image


ROOT = Path(__file__).resolve().parents[2]
MANIFEST_PATH = ROOT / "tools" / "cat_manifest.json"
OUTPUT = ROOT / "assets" / "cats"
NITROFS = ROOT / "nitrofs" / "cats"


class AssetManifestTest(unittest.TestCase):
    def test_manifest_processed_pngs_and_nitrofs_textures_match_contract(self) -> None:
        """All mapped action sprites must have review PNGs and runtime binaries."""
        manifest = json.loads(MANIFEST_PATH.read_text(encoding="utf-8"))
        self.assertEqual(
            manifest["cats"],
            {"a": "orange", "b": "tabby", "c": "chouju", "d": "maodie", "e": "banana"},
        )
        self.assertEqual(
            manifest["actions"],
            {
                "1": "yowl",
                "2": "hiss",
                "3": "scratch",
                "4": "hit",
                "5": "heal",
                "6": "dead",
                "7": "idle",
            },
        )

        cats = manifest["cats"]
        actions = manifest["actions"]
        expected_stems = {
            f"{cat_name}_{action_name}"
            for cat_name in cats.values()
            for action_name in actions.values()
        }
        self.assertEqual(
            {path.stem for path in OUTPUT.glob("*.png")}, expected_stems
        )

        for stem in expected_stems:
            with Image.open(OUTPUT / f"{stem}.png") as image:
                self.assertEqual(image.size, (128, 128))
                self.assertEqual(image.mode, "RGBA")
                self.assertIsNotNone(image.getbbox())

            image_data = NITROFS / f"{stem}.img.bin"
            palette_data = NITROFS / f"{stem}.pal.bin"
            self.assertEqual(image_data.stat().st_size, 128 * 128)
            self.assertGreaterEqual(palette_data.stat().st_size, 2)
            self.assertLessEqual(palette_data.stat().st_size, 256 * 2)
            self.assertEqual(palette_data.stat().st_size % 2, 0)
            self.assertLess(max(image_data.read_bytes()), palette_data.stat().st_size // 2)


if __name__ == "__main__":
    unittest.main()
