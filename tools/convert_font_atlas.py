#!/usr/bin/env python3
"""Convert the generated grayscale font atlas to an NDS A5I3 texture."""

from __future__ import annotations

import argparse
from pathlib import Path

from PIL import Image


MAX_TEXTURE_DIMENSION = 1024
PALETTE_ENTRIES = 8
WHITE_BGR555 = 0x7FFF


def next_power_of_two(value: int) -> int:
    if value <= 0:
        raise ValueError("texture dimension must be positive")
    return 1 << (value - 1).bit_length()


def encode_a5i3(alpha: int) -> int:
    if alpha == 0:
        return 0
    alpha5 = max(1, (alpha * 31 + 127) // 255)
    return (alpha5 << 3) | 1


def convert(input_atlas: Path, output_image: Path, output_palette: Path) -> None:
    with Image.open(input_atlas) as source:
        atlas = source.convert("L")
        width, height = atlas.size

    texture_width = next_power_of_two(width)
    texture_height = next_power_of_two(height)
    if texture_width != width:
        raise ValueError(f"font atlas width must already be a power of two: {width}")
    if texture_width > MAX_TEXTURE_DIMENSION or texture_height > MAX_TEXTURE_DIMENSION:
        raise ValueError(
            f"font atlas exceeds NDS texture bounds: {texture_width}x{texture_height}"
        )

    image_data = bytearray(texture_width * texture_height)
    source_data = atlas.tobytes()
    image_data[: len(source_data)] = (encode_a5i3(alpha) for alpha in source_data)

    palette = [0] + [WHITE_BGR555] * (PALETTE_ENTRIES - 1)
    palette_data = b"".join(color.to_bytes(2, "little") for color in palette)

    output_image.parent.mkdir(parents=True, exist_ok=True)
    output_palette.parent.mkdir(parents=True, exist_ok=True)
    output_image.write_bytes(image_data)
    output_palette.write_bytes(palette_data)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--input-atlas", type=Path, required=True)
    parser.add_argument("--output-image", type=Path, required=True)
    parser.add_argument("--output-palette", type=Path, required=True)
    arguments = parser.parse_args()
    convert(arguments.input_atlas, arguments.output_image, arguments.output_palette)


if __name__ == "__main__":
    main()
