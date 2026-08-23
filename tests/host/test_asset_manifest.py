"""Validate the deterministic cat-asset manifest and generated runtime artifacts."""

from __future__ import annotations

import json
from pathlib import Path
import sys
import unittest

from PIL import Image


ROOT = Path(__file__).resolve().parents[2]
MANIFEST_PATH = ROOT / "tools" / "cat_manifest.json"
OUTPUT = ROOT / "assets" / "cats"
NITROFS = ROOT / "nitrofs" / "cats"
SOURCE = ROOT / "assets_src" / "cats"
sys.path.insert(0, str(ROOT / "tools"))

from process_cats import indexed_texture


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
        expected_sources = {
            f"cat_{cat}_{action}.png" for cat in cats for action in actions
        }
        self.assertEqual({path.name for path in SOURCE.glob("*.png")}, expected_sources)
        self.assertEqual(len(list(SOURCE.glob("*.png"))), 35)
        self.assertEqual({path.stem for path in OUTPUT.glob("*.png")}, expected_stems)
        self.assertEqual({path.stem for path in OUTPUT.glob("*.grit")}, expected_stems)
        self.assertEqual(len(list(OUTPUT.glob("*.png"))), 35)
        self.assertEqual(len(list(OUTPUT.glob("*.grit"))), 35)
        self.assertEqual(
            {path.name for path in NITROFS.glob("*.img.bin")},
            {f"{stem}.img.bin" for stem in expected_stems},
        )
        self.assertEqual(
            {path.name for path in NITROFS.glob("*.pal.bin")},
            {f"{stem}.pal.bin" for stem in expected_stems},
        )
        self.assertEqual(len(list(NITROFS.glob("*.img.bin"))), 35)
        self.assertEqual(len(list(NITROFS.glob("*.pal.bin"))), 35)

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

    def test_retained_semitransparent_edge_uses_straight_rgb_not_magenta_matte(self) -> None:
        """An edge retained after thresholding must not inherit the magenta key color."""
        original = (20, 80, 140, 128)
        texture, palette = indexed_texture(Image.new("RGBA", (1, 1), original))
        self.assertNotEqual(texture[0], 0)
        color = int.from_bytes(palette[texture[0] * 2 : texture[0] * 2 + 2], "little")
        decoded = ((color & 0x1F) << 3, ((color >> 5) & 0x1F) << 3, ((color >> 10) & 0x1F) << 3)
        self.assertTrue(all(abs(actual - expected) <= 8 for actual, expected in zip(decoded, original)))
        self.assertFalse(decoded[0] > 200 and decoded[1] < 50 and decoded[2] > 200)


if __name__ == "__main__":
    unittest.main()
