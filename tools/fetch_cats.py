"""Fetch the immutable source cat images described by cat_manifest.json."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
from urllib.error import HTTPError, URLError
from urllib.request import Request, urlopen


PNG_SIGNATURE = b"\x89PNG\r\n\x1a\n"
ROOT = Path(__file__).resolve().parents[1]


def load_manifest(path: Path) -> dict[str, object]:
    return json.loads(path.read_text(encoding="utf-8"))


def fetch(url: str, destination: Path) -> None:
    request = Request(url, headers={"User-Agent": "JiMiDoo asset pipeline/1.0"})
    try:
        with urlopen(request, timeout=30) as response:
            if response.status != 200:
                raise RuntimeError(f"unexpected HTTP status {response.status} for {url}")
            payload = response.read()
    except HTTPError as error:
        raise RuntimeError(f"HTTP {error.code} while fetching {url}") from error
    except URLError as error:
        raise RuntimeError(f"network error while fetching {url}: {error.reason}") from error

    if not payload.startswith(PNG_SIGNATURE):
        raise RuntimeError(f"downloaded data is not a PNG: {url}")

    temporary = destination.with_suffix(destination.suffix + ".part")
    temporary.write_bytes(payload)
    temporary.replace(destination)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--manifest", type=Path, default=ROOT / "tools" / "cat_manifest.json")
    parser.add_argument("--output", type=Path, default=ROOT / "assets_src" / "cats")
    args = parser.parse_args()

    manifest = load_manifest(args.manifest)
    template = str(manifest["source_url_template"])
    cats = dict(manifest["cats"])
    actions = dict(manifest["actions"])
    args.output.mkdir(parents=True, exist_ok=True)

    for cat in sorted(cats):
        for action in sorted(actions, key=int):
            destination = args.output / f"cat_{cat}_{action}.png"
            if destination.exists():
                print(f"keep {destination}")
                continue
            url = template.format(cat=cat, action=action)
            print(f"fetch {url}")
            fetch(url, destination)


if __name__ == "__main__":
    main()
