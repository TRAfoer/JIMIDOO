#!/usr/bin/env python3
"""Extract the unique characters used by both UTF-8 localization tables."""

from __future__ import annotations

import argparse
from pathlib import Path
import re


ROOT = Path(__file__).resolve().parents[1]
TABLES = (
    ROOT / "source" / "localization" / "strings_zh_cn.c",
    ROOT / "source" / "localization" / "strings_en.c",
)
DEFAULT_OUTPUT = ROOT / "assets" / "fonts" / "required_glyphs.txt"
ENTRY = re.compile(r'^\s*\[TEXT_[A-Z0-9_]+\]\s*=\s*"((?:\\.|[^"\\])*)"', re.MULTILINE)


def decode_c_string(value: str) -> str:
    """Decode the small escape subset used by ordinary C string literals."""
    decoded: list[str] = []
    index = 0
    escapes = {"n": "\n", "r": "\r", "t": "\t", "\\": "\\", '"': '"'}
    while index < len(value):
        character = value[index]
        if character != "\\":
            decoded.append(character)
            index += 1
            continue
        if index + 1 >= len(value):
            raise ValueError("unterminated C escape")
        escaped = value[index + 1]
        decoded.append(escapes.get(escaped, escaped))
        index += 2
    return "".join(decoded)


def collect_glyphs(tables: tuple[Path, ...] = TABLES) -> str:
    characters: set[str] = set()
    for table in tables:
        source = table.read_text(encoding="utf-8")
        entries = ENTRY.findall(source)
        if not entries:
            raise ValueError(f"no localization entries found in {table}")
        for entry in entries:
            characters.update(decode_c_string(entry))
    return "".join(sorted(characters, key=ord))


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", type=Path, default=DEFAULT_OUTPUT)
    arguments = parser.parse_args()
    glyphs = collect_glyphs()
    arguments.output.parent.mkdir(parents=True, exist_ok=True)
    arguments.output.write_text(glyphs + "\n", encoding="utf-8", newline="\n")


if __name__ == "__main__":
    main()
