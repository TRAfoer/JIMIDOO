"""Exercise the generated font artifacts rather than their source text."""

from __future__ import annotations

import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile
import unittest

from fontTools.ttLib import TTFont
from PIL import Image


ROOT = Path(__file__).resolve().parents[2]
DEFAULT_FONT = Path(r"C:\Windows\Fonts\simhei.ttf")


class FontArtifactTest(unittest.TestCase):
    def build(self, output: Path) -> tuple[Path, Path, Path]:
        glyphs = output / "required_glyphs.txt"
        subset = output / "jimidou_subset.ttf"
        atlas = output / "jimidou_font_atlas.png"
        metrics = output / "jimidou_font_metrics.h"
        environment = os.environ.copy()
        environment["FONT_FILE"] = str(DEFAULT_FONT)
        subprocess.run(
            [sys.executable, "tools/extract_glyphs.py", "--output", str(glyphs)],
            cwd=ROOT,
            env=environment,
            check=True,
        )
        subprocess.run(
            [
                sys.executable,
                "tools/build_font.py",
                "--glyphs",
                str(glyphs),
                "--output-font",
                str(subset),
                "--output-atlas",
                str(atlas),
                "--output-metrics",
                str(metrics),
            ],
            cwd=ROOT,
            env=environment,
            check=True,
        )
        return subset, atlas, metrics

    def test_subset_maps_every_required_glyph_deterministically(self) -> None:
        """A missing cmap entry or non-repeatable asset must fail this test."""
        with tempfile.TemporaryDirectory() as temporary:
            first_dir = Path(temporary) / "first"
            second_dir = Path(temporary) / "second"
            first_dir.mkdir()
            second_dir.mkdir()
            first_font, first_atlas, first_metrics = self.build(first_dir)
            second_font, second_atlas, second_metrics = self.build(second_dir)

            self.assertEqual(first_font.read_bytes(), second_font.read_bytes())
            self.assertEqual(first_atlas.read_bytes(), second_atlas.read_bytes())
            self.assertEqual(first_metrics.read_bytes(), second_metrics.read_bytes())

            required = (first_dir / "required_glyphs.txt").read_text(encoding="utf-8").rstrip("\n")
            cmap = TTFont(first_font).getBestCmap()
            mapped = {
                int(codepoint, 16)
                for codepoint in re.findall(r"\{ 0x([0-9A-F]{4,6}),", first_metrics.read_text(encoding="utf-8"))
            }
            self.assertTrue(required)
            self.assertTrue(all(ord(character) in cmap for character in required))
            self.assertTrue(all(ord(character) in mapped for character in required))
            with Image.open(first_atlas) as atlas:
                self.assertGreater(atlas.width, 0)
                self.assertGreater(atlas.height, 0)


if __name__ == "__main__":
    unittest.main()
