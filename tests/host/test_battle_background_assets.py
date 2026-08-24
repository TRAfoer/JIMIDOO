"""Validate the compact battle-background asset contract."""

from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

from PIL import Image


ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / "assets_src" / "backgrounds"
OUTPUT = ROOT / "assets" / "backgrounds"
NITROFS = ROOT / "nitrofs" / "backgrounds"
STEMS = {"alley_day", "alley_dusk", "alley_rain"}


class BattleBackgroundAssetsTest(unittest.TestCase):
    def test_three_alley_backgrounds_fit_one_runtime_texture_slot(self) -> None:
        self.assertEqual({path.stem for path in SOURCE.glob("*.png")}, STEMS)
        self.assertEqual({path.stem for path in OUTPUT.glob("*.png")}, STEMS)
        self.assertEqual(
            {path.name for path in NITROFS.glob("*.img.bin")},
            {f"{stem}.img.bin" for stem in STEMS},
        )
        self.assertEqual(
            {path.name for path in NITROFS.glob("*.pal.bin")},
            {f"{stem}.pal.bin" for stem in STEMS},
        )
        for stem in STEMS:
            with Image.open(OUTPUT / f"{stem}.png") as image:
                self.assertEqual(image.mode, "RGB")
                self.assertEqual(image.size, (128, 96))
            self.assertEqual(
                (NITROFS / f"{stem}.img.bin").stat().st_size,
                128 * 128,
            )
            palette_size = (NITROFS / f"{stem}.pal.bin").stat().st_size
            self.assertEqual(palette_size, 256 * 2)

    def test_background_outputs_are_reproducible_from_sources(self) -> None:
        generator = ROOT / "tools" / "process_battle_backgrounds.py"
        self.assertTrue(generator.is_file())
        with tempfile.TemporaryDirectory() as directory:
            temporary = Path(directory)
            output = temporary / "review"
            nitrofs = temporary / "nitrofs"
            subprocess.run(
                [
                    sys.executable,
                    str(generator),
                    "--source",
                    str(SOURCE),
                    "--output",
                    str(output),
                    "--nitrofs",
                    str(nitrofs),
                ],
                check=True,
            )
            for stem in STEMS:
                self.assertEqual(
                    (output / f"{stem}.png").read_bytes(),
                    (OUTPUT / f"{stem}.png").read_bytes(),
                )
                for suffix in ("img.bin", "pal.bin"):
                    self.assertEqual(
                        (nitrofs / f"{stem}.{suffix}").read_bytes(),
                        (NITROFS / f"{stem}.{suffix}").read_bytes(),
                    )


if __name__ == "__main__":
    unittest.main()
