"""Validate the NitroFS font texture consumed by the NDS renderers."""

from __future__ import annotations

import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile
import unittest

from PIL import Image


ROOT = Path(__file__).resolve().parents[2]
DEFAULT_FONT = Path(r"C:\Windows\Fonts\simhei.ttf")
ATLAS = ROOT / "assets" / "fonts" / "jimidou_font_atlas.png"
METRICS = ROOT / "include" / "generated" / "jimidou_font_metrics.h"
RUNTIME_IMAGE = ROOT / "nitrofs" / "fonts" / "jimidou_font.a5i3.bin"
RUNTIME_PALETTE = ROOT / "nitrofs" / "fonts" / "jimidou_font.pal.bin"
CAT_IMAGES = ROOT / "nitrofs" / "cats"
FONT_TEXTURE_LIMIT = 128 * 1024
TEXTURE_VRAM_LIMIT = 3 * 128 * 1024
CAT_CACHE_ACTION_LIMIT = 2 * 7
MANDATORY_RUNTIME_GLYPHS = set("0123456789%+.-/:")
METRIC = re.compile(
    r"\{ 0x([0-9A-F]{4,6}), (-?\d+), (-?\d+), (-?\d+), (-?\d+), "
    r"(-?\d+), (-?\d+), (-?\d+) \},"
)


class FontRuntimeArtifactTest(unittest.TestCase):
    def build_catalog_font(self, output: Path) -> tuple[Path, Path]:
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
        return atlas, metrics

    def convert(self, output: Path, atlas: Path) -> tuple[Path, Path]:
        image = output / "jimidou_font.a5i3.bin"
        palette = output / "jimidou_font.pal.bin"
        subprocess.run(
            [
                sys.executable,
                "tools/convert_font_atlas.py",
                "--input-atlas",
                str(atlas),
                "--output-image",
                str(image),
                "--output-palette",
                str(palette),
            ],
            cwd=ROOT,
            check=True,
        )
        return image, palette

    def test_a5i3_runtime_texture_is_deterministic_bounded_and_covered(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            first = Path(temporary) / "first"
            second = Path(temporary) / "second"
            first.mkdir()
            second.mkdir()
            generated = Path(temporary) / "generated"
            generated.mkdir()
            fresh_atlas, fresh_metrics = self.build_catalog_font(generated)
            first_image, first_palette = self.convert(first, fresh_atlas)
            second_image, second_palette = self.convert(second, fresh_atlas)

            image_data = first_image.read_bytes()
            palette_data = first_palette.read_bytes()
            cat_payload_sizes = sorted(
                (path.stat().st_size for path in CAT_IMAGES.glob("*.img.bin")),
                reverse=True,
            )
            self.assertEqual(len(cat_payload_sizes), 35)
            self.assertLessEqual(len(image_data), FONT_TEXTURE_LIMIT)
            self.assertLessEqual(
                len(image_data) + sum(cat_payload_sizes[:CAT_CACHE_ACTION_LIMIT]),
                TEXTURE_VRAM_LIMIT,
            )
            self.assertEqual(image_data, second_image.read_bytes())
            self.assertEqual(palette_data, second_palette.read_bytes())
            self.assertEqual(image_data, RUNTIME_IMAGE.read_bytes())
            self.assertEqual(palette_data, RUNTIME_PALETTE.read_bytes())
            self.assertEqual(fresh_atlas.read_bytes(), ATLAS.read_bytes())
            self.assertEqual(fresh_metrics.read_bytes(), METRICS.read_bytes())

            with Image.open(fresh_atlas) as atlas:
                width, height = atlas.size
                texture_height = 1 << (height - 1).bit_length()
                self.assertEqual(len(image_data), width * texture_height)
                self.assertEqual(set(image_data[width * height :]), {0})
                samples = atlas.convert("L").tobytes()
                expected = bytes(
                    0 if sample == 0
                    else (max(1, (sample * 31 + 127) // 255) << 3) | 1
                    for sample in samples
                )
                self.assertEqual(image_data[: width * height], expected)

            self.assertEqual(len(palette_data), 8 * 2)
            self.assertEqual(int.from_bytes(palette_data[:2], "little"), 0)
            self.assertEqual(
                [int.from_bytes(palette_data[index : index + 2], "little")
                 for index in range(2, len(palette_data), 2)],
                [0x7FFF] * 7,
            )

            metrics = [
                (int(values[0], 16), *(int(value) for value in values[1:]))
                for values in METRIC.findall(fresh_metrics.read_text(encoding="utf-8"))
            ]
            self.assertTrue(metrics)
            metric_codepoints = {codepoint for codepoint, *_ in metrics}
            self.assertTrue(
                {ord(character) for character in MANDATORY_RUNTIME_GLYPHS}
                <= metric_codepoints
            )
            for codepoint, x, y, glyph_width, glyph_height, _, _, _ in metrics:
                if chr(codepoint).isspace():
                    continue
                rows = (
                    image_data[row * width + x : row * width + x + glyph_width]
                    for row in range(y, y + glyph_height)
                )
                self.assertTrue(
                    any(pixel >> 3 for row in rows for pixel in row),
                    f"U+{codepoint:04X} has no runtime alpha coverage",
                )

    def test_rom_build_depends_on_runtime_font_artifacts(self) -> None:
        makefile = (ROOT / "Makefile").read_text(encoding="utf-8")
        self.assertRegex(
            makefile,
            r"(?m)^\$\(ROM\):\s+\$\(FONT_RUNTIME_ASSETS\)\s*$",
        )


if __name__ == "__main__":
    unittest.main()
