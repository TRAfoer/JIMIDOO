"""Exercise the generated font artifacts rather than their source text."""

from __future__ import annotations

import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))

from fontTools.ttLib import TTFont
from PIL import Image

from tools.extract_glyphs import collect_glyphs


DEFAULT_FONT = Path(r"C:\Windows\Fonts\simhei.ttf")
COMMITTED_GLYPHS = ROOT / "assets" / "fonts" / "required_glyphs.txt"
COMMITTED_SUBSET = ROOT / "assets" / "fonts" / "jimidou_subset.ttf"
COMMITTED_ATLAS = ROOT / "assets" / "fonts" / "jimidou_font_atlas.png"
COMMITTED_METRICS = ROOT / "include" / "generated" / "jimidou_font_metrics.h"
METRIC = re.compile(
    r"\{ 0x([0-9A-F]{4,6}), (-?\d+), (-?\d+), (-?\d+), (-?\d+), (-?\d+), (-?\d+), (-?\d+) \},"
)
MANDATORY_RUNTIME_GLYPHS = set("0123456789%+.-/:")


class FontArtifactTest(unittest.TestCase):
    def test_runtime_glyphs_are_seeded_independently_of_catalog_text(self) -> None:
        """Removing the mandatory seed must not silently strip runtime values."""
        with tempfile.TemporaryDirectory() as temporary:
            catalog = Path(temporary) / "catalog.c"
            catalog.write_text('[TEXT_ONLY] = "A";\n', encoding="utf-8")
            glyphs = set(collect_glyphs((catalog,)))
            self.assertTrue(MANDATORY_RUNTIME_GLYPHS <= glyphs)

    def build(self, output: Path) -> tuple[Path, Path, Path, Path]:
        glyphs = output / "required_glyphs.txt"
        subset = output / "jimidou_subset.ttf"
        atlas = output / "jimidou_font_atlas.png"
        metrics = output / "jimidou_font_metrics.h"
        environment = os.environ.copy()
        environment.setdefault("FONT_FILE", str(DEFAULT_FONT))
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
        return glyphs, subset, atlas, metrics

    def test_subset_maps_every_required_glyph_deterministically(self) -> None:
        """A missing cmap entry or non-repeatable asset must fail this test."""
        with tempfile.TemporaryDirectory() as temporary:
            first_dir = Path(temporary) / "first"
            second_dir = Path(temporary) / "second"
            first_dir.mkdir()
            second_dir.mkdir()
            first_glyphs, first_font, first_atlas, first_metrics = self.build(first_dir)
            second_glyphs, second_font, second_atlas, second_metrics = self.build(second_dir)

            self.assertEqual(first_glyphs.read_bytes(), second_glyphs.read_bytes())
            self.assertEqual(first_font.read_bytes(), second_font.read_bytes())
            self.assertEqual(first_atlas.read_bytes(), second_atlas.read_bytes())
            self.assertEqual(first_metrics.read_bytes(), second_metrics.read_bytes())
            self.assertEqual(first_glyphs.read_bytes(), COMMITTED_GLYPHS.read_bytes())
            self.assertEqual(first_font.read_bytes(), COMMITTED_SUBSET.read_bytes())
            self.assertEqual(first_atlas.read_bytes(), COMMITTED_ATLAS.read_bytes())
            self.assertEqual(first_metrics.read_bytes(), COMMITTED_METRICS.read_bytes())

            required = (first_dir / "required_glyphs.txt").read_text(encoding="utf-8").rstrip("\n")
            self.assertTrue(MANDATORY_RUNTIME_GLYPHS <= set(required))
            cmap = TTFont(first_font).getBestCmap()
            metrics = [
                (int(values[0], 16), *(int(value) for value in values[1:]))
                for values in METRIC.findall(first_metrics.read_text(encoding="utf-8"))
            ]
            metric_by_codepoint = {codepoint: metric for codepoint, *metric in metrics}
            self.assertTrue(required)
            self.assertTrue(all(ord(character) in cmap for character in required))
            self.assertEqual(len(metrics), len(required))
            self.assertEqual(len(metric_by_codepoint), len(metrics))
            self.assertTrue(all(ord(character) in metric_by_codepoint for character in required))
            with Image.open(first_atlas) as atlas:
                self.assertGreater(atlas.width, 0)
                self.assertGreater(atlas.height, 0)
                for character in required:
                    x, y, width, height, _, _, _ = metric_by_codepoint[ord(character)]
                    self.assertGreaterEqual(x, 0)
                    self.assertGreaterEqual(y, 0)
                    self.assertGreaterEqual(width, 0)
                    self.assertGreaterEqual(height, 0)
                    self.assertLessEqual(x + width, atlas.width)
                    self.assertLessEqual(y + height, atlas.height)
                    if not character.isspace():
                        self.assertGreater(width, 0)
                        self.assertGreater(height, 0)
                        self.assertIsNotNone(atlas.crop((x, y, x + width, y + height)).getbbox())


if __name__ == "__main__":
    unittest.main()
