"""Generate review PNGs and NitroFS indexed texture assets from source cats."""

from __future__ import annotations

import argparse
import json
from pathlib import Path

from PIL import Image, ImageFilter


ROOT = Path(__file__).resolve().parents[1]
CANVAS_SIZE = 128
VISIBLE_SIZE = 112
PALETTE_COLORS = 256
TRANSPARENT_INDEX = 0
TRANSPARENT_RGB = (255, 0, 255)
GRIT_OPTIONS = "-g -gTFF00FF -gt -gB8 -m! -p!"


def load_manifest(path: Path) -> dict[str, object]:
    return json.loads(path.read_text(encoding="utf-8"))


def process_image(source: Path) -> Image.Image:
    """Return one centered RGBA review image without changing the source file."""
    with Image.open(source) as opened:
        image = opened.convert("RGBA")
    alpha_bounds = image.getchannel("A").getbbox()
    if alpha_bounds is None:
        raise ValueError(f"source has no visible pixels: {source}")
    image = image.crop(alpha_bounds)
    scale = VISIBLE_SIZE / max(image.size)
    resized = image.resize(
        (max(1, round(image.width * scale)), max(1, round(image.height * scale))),
        Image.Resampling.LANCZOS,
    )
    sharpened = resized.filter(ImageFilter.UnsharpMask(radius=1.25, percent=110, threshold=2))
    canvas = Image.new("RGBA", (CANVAS_SIZE, CANVAS_SIZE), (0, 0, 0, 0))
    position = ((CANVAS_SIZE - sharpened.width) // 2, (CANVAS_SIZE - sharpened.height) // 2)
    canvas.alpha_composite(sharpened, position)
    return canvas


def indexed_texture(image: Image.Image) -> tuple[bytes, bytes]:
    """Encode RGBA review art as index-0-transparent RGB256 texture data.

    The image data is one row-major byte per texel.  The palette is 256 little-
    endian Nintendo DS BGR555 colors; index zero is the magenta transparency key.
    """
    rgba = image.convert("RGBA")
    alpha = rgba.getchannel("A")
    keyed = Image.new("RGB", rgba.size, TRANSPARENT_RGB)
    keyed.paste(rgba.convert("RGB"), mask=alpha)
    quantized = keyed.quantize(
        colors=PALETTE_COLORS - 1,
        method=Image.Quantize.MEDIANCUT,
        dither=Image.Dither.NONE,
    )
    palette = quantized.getpalette()[: (PALETTE_COLORS - 1) * 3]
    indices = bytearray(quantized.tobytes())
    alpha_bytes = alpha.tobytes()
    for offset, value in enumerate(alpha_bytes):
        indices[offset] = TRANSPARENT_INDEX if value < 128 else indices[offset] + 1

    ds_palette = bytearray()
    for red, green, blue in [TRANSPARENT_RGB, *zip(*[iter(palette)] * 3)]:
        color = (red >> 3) | ((green >> 3) << 5) | ((blue >> 3) << 10)
        ds_palette.extend(color.to_bytes(2, "little"))
    return bytes(indices), bytes(ds_palette)


def write_outputs(image: Image.Image, stem: str, output: Path, nitrofs: Path) -> None:
    output.mkdir(parents=True, exist_ok=True)
    nitrofs.mkdir(parents=True, exist_ok=True)
    image.save(output / f"{stem}.png")
    (output / f"{stem}.grit").write_text(GRIT_OPTIONS + "\n", encoding="ascii", newline="\n")
    texture, palette = indexed_texture(image)
    (nitrofs / f"{stem}.img.bin").write_bytes(texture)
    (nitrofs / f"{stem}.pal.bin").write_bytes(palette)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--manifest", type=Path, default=ROOT / "tools" / "cat_manifest.json")
    parser.add_argument("--source", type=Path, default=ROOT / "assets_src" / "cats")
    parser.add_argument("--output", type=Path, default=ROOT / "assets" / "cats")
    parser.add_argument("--nitrofs", type=Path, default=ROOT / "nitrofs" / "cats")
    args = parser.parse_args()

    manifest = load_manifest(args.manifest)
    cats = dict(manifest["cats"])
    actions = dict(manifest["actions"])
    for cat, cat_name in sorted(cats.items()):
        for action, action_name in sorted(actions.items(), key=lambda item: int(item[0])):
            source = args.source / f"cat_{cat}_{action}.png"
            if not source.is_file():
                raise FileNotFoundError(f"missing source image: {source}")
            write_outputs(process_image(source), f"{cat_name}_{action_name}", args.output, args.nitrofs)


if __name__ == "__main__":
    main()
