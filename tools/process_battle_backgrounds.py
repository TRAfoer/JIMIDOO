"""Build compact RGB256 NitroFS textures for battle alley backgrounds."""

from __future__ import annotations

import argparse
from pathlib import Path

from PIL import Image, ImageOps


ROOT = Path(__file__).resolve().parents[1]
STEMS = ("alley_day", "alley_dusk", "alley_rain")
VISIBLE_SIZE = (128, 96)
TEXTURE_SIZE = (128, 128)


def process_background(source: Path) -> Image.Image:
    """Return a deterministic 4:3 RGB review image for the NDS screen."""
    with Image.open(source) as opened:
        image = ImageOps.exif_transpose(opened).convert("RGB")
    return ImageOps.fit(
        image,
        VISIBLE_SIZE,
        method=Image.Resampling.LANCZOS,
        centering=(0.5, 0.5),
    )


def indexed_texture(image: Image.Image) -> tuple[bytes, bytes]:
    """Encode a 128x96 image in a padded 128x128 RGB256 texture."""
    quantized = image.quantize(
        colors=256,
        method=Image.Quantize.MEDIANCUT,
        dither=Image.Dither.NONE,
    )
    indices = bytearray(TEXTURE_SIZE[0] * TEXTURE_SIZE[1])
    visible = quantized.tobytes()
    for row in range(VISIBLE_SIZE[1]):
        start = row * TEXTURE_SIZE[0]
        indices[start : start + VISIBLE_SIZE[0]] = visible[
            row * VISIBLE_SIZE[0] : (row + 1) * VISIBLE_SIZE[0]
        ]

    palette = quantized.getpalette()[: 256 * 3]
    ds_palette = bytearray()
    for red, green, blue in zip(*[iter(palette)] * 3):
        color = (red >> 3) | ((green >> 3) << 5) | ((blue >> 3) << 10)
        ds_palette.extend(color.to_bytes(2, "little"))
    return bytes(indices), bytes(ds_palette)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--source", type=Path, default=ROOT / "assets_src" / "backgrounds"
    )
    parser.add_argument(
        "--output", type=Path, default=ROOT / "assets" / "backgrounds"
    )
    parser.add_argument(
        "--nitrofs", type=Path, default=ROOT / "nitrofs" / "backgrounds"
    )
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    args.nitrofs.mkdir(parents=True, exist_ok=True)

    for stem in STEMS:
        source = args.source / f"{stem}.png"
        if not source.is_file():
            raise FileNotFoundError(f"missing background source: {source}")
        image = process_background(source)
        image.save(args.output / f"{stem}.png", optimize=False)
        texture, palette = indexed_texture(image)
        (args.nitrofs / f"{stem}.img.bin").write_bytes(texture)
        (args.nitrofs / f"{stem}.pal.bin").write_bytes(palette)


if __name__ == "__main__":
    main()
