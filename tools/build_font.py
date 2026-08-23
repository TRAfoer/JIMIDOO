#!/usr/bin/env python3
"""Build deterministic subset-font, atlas, and metrics assets for JiMiDoo."""

from __future__ import annotations

import argparse
import os
from pathlib import Path

from fontTools import subset
from fontTools.ttLib import TTFont
from PIL import Image, ImageDraw, ImageFont


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_FONT = Path(r"C:\Windows\Fonts\simhei.ttf")
DEFAULT_GLYPHS = ROOT / "assets" / "fonts" / "required_glyphs.txt"
DEFAULT_SUBSET = ROOT / "assets" / "fonts" / "jimidou_subset.ttf"
DEFAULT_ATLAS = ROOT / "assets" / "fonts" / "jimidou_font_atlas.png"
DEFAULT_METRICS = ROOT / "include" / "generated" / "jimidou_font_metrics.h"
ATLAS_WIDTH = 1024
PADDING = 1
PIXEL_SIZE = 32


def glyph_characters(path: Path) -> list[str]:
    characters = path.read_text(encoding="utf-8").rstrip("\n")
    if not characters:
        raise ValueError(f"glyph list is empty: {path}")
    return sorted(set(characters), key=ord)


def require_coverage(font_path: Path, characters: list[str]) -> None:
    cmap = TTFont(font_path, recalcTimestamp=False).getBestCmap()
    missing = [character for character in characters if ord(character) not in cmap]
    if missing:
        codepoints = ", ".join(f"U+{ord(character):04X}" for character in missing)
        raise ValueError(f"font lacks required glyphs: {codepoints}")


def build_subset(font_path: Path, characters: list[str], output: Path) -> None:
    font = TTFont(font_path, recalcTimestamp=False)
    options = subset.Options()
    options.recalc_timestamp = False
    options.name_IDs = [0, 1, 2, 3, 4, 5, 6]
    options.drop_tables.append("meta")
    subsetter = subset.Subsetter(options=options)
    subsetter.populate(unicodes=[ord(character) for character in characters])
    subsetter.subset(font)
    font.recalcTimestamp = False
    font["head"].created = 0
    font["head"].modified = 0
    output.parent.mkdir(parents=True, exist_ok=True)
    font.save(output)


def glyph_metrics(font_path: Path, characters: list[str]) -> tuple[Image.Image, list[tuple[int, int, int, int, int, int, int, int]]]:
    font = ImageFont.truetype(str(font_path), PIXEL_SIZE)
    placements: list[tuple[int, int, int, int, int, int, int, int]] = []
    x = PADDING
    y = PADDING
    row_height = 0
    for character in characters:
        left, top, right, bottom = font.getbbox(character)
        width = max(right - left, 0)
        height = max(bottom - top, 0)
        cell_width = max(width, 1)
        cell_height = max(height, 1)
        if x + cell_width + PADDING > ATLAS_WIDTH:
            x = PADDING
            y += row_height + PADDING
            row_height = 0
        advance = int(round(font.getlength(character)))
        placements.append((ord(character), x, y, width, height, advance, left, top))
        x += cell_width + PADDING
        row_height = max(row_height, cell_height)

    atlas_height = y + row_height + PADDING
    atlas = Image.new("L", (ATLAS_WIDTH, atlas_height), 0)
    draw = ImageDraw.Draw(atlas)
    for character, placement in zip(characters, placements):
        _, glyph_x, glyph_y, _, _, _, left, _ = placement
        _, top, _, _ = font.getbbox(character)
        draw.text((glyph_x - left, glyph_y - top), character, font=font, fill=255)
    return atlas, placements


def write_metrics(output: Path, atlas_height: int, placements: list[tuple[int, int, int, int, int, int, int, int]]) -> None:
    lines = [
        "#ifndef JIMIDOU_FONT_METRICS_H",
        "#define JIMIDOU_FONT_METRICS_H",
        "",
        "#include <stdint.h>",
        "",
        "typedef struct JimiDooGlyphMetric {",
        "    uint32_t codepoint;",
        "    uint16_t x;",
        "    uint16_t y;",
        "    uint16_t width;",
        "    uint16_t height;",
        "    int16_t advance_x;",
        "    int16_t bearing_x;",
        "    int16_t bearing_y;",
        "} JimiDooGlyphMetric;",
        "",
        f"#define JIMIDOO_FONT_ATLAS_WIDTH {ATLAS_WIDTH}",
        f"#define JIMIDOO_FONT_ATLAS_HEIGHT {atlas_height}",
        f"#define JIMIDOO_FONT_GLYPH_COUNT {len(placements)}",
        "",
        "static const JimiDooGlyphMetric jimidou_font_glyphs[JIMIDOO_FONT_GLYPH_COUNT] = {",
    ]
    for codepoint, x, y, width, height, advance, bearing_x, bearing_y in placements:
        lines.append(
            f"    {{ 0x{codepoint:04X}, {x}, {y}, {width}, {height}, {advance}, {bearing_x}, {bearing_y} }},"
        )
    lines.extend(["};", "", "#endif", ""])
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text("\n".join(lines), encoding="utf-8", newline="\n")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--font", type=Path, default=Path(os.environ.get("FONT_FILE", DEFAULT_FONT)))
    parser.add_argument("--glyphs", type=Path, default=DEFAULT_GLYPHS)
    parser.add_argument("--output-font", type=Path, default=DEFAULT_SUBSET)
    parser.add_argument("--output-atlas", type=Path, default=DEFAULT_ATLAS)
    parser.add_argument("--output-metrics", type=Path, default=DEFAULT_METRICS)
    arguments = parser.parse_args()

    if not arguments.font.is_file():
        raise FileNotFoundError(f"FONT_FILE does not exist: {arguments.font}")
    characters = glyph_characters(arguments.glyphs)
    require_coverage(arguments.font, characters)
    build_subset(arguments.font, characters, arguments.output_font)
    atlas, placements = glyph_metrics(arguments.output_font, characters)
    arguments.output_atlas.parent.mkdir(parents=True, exist_ok=True)
    atlas.save(arguments.output_atlas, format="PNG", optimize=False)
    write_metrics(arguments.output_metrics, atlas.height, placements)


if __name__ == "__main__":
    main()
