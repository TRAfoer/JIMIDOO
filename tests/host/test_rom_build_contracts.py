"""Check release/build contracts that are independent of ARM execution."""

from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[2]


class RomBuildContractTest(unittest.TestCase):
    def test_empty_release_metadata_produces_an_empty_banner_string(self) -> None:
        makefile = (ROOT / "Makefile").read_text(encoding="utf-8")
        self.assertIn(
            "ifeq ($(strip $(GAME_TITLE)$(GAME_SUBTITLE)$(GAME_AUTHOR)),)",
            makefile,
        )
        self.assertIn("    GAME_FULL_TITLE :=\n", makefile)


if __name__ == "__main__":
    unittest.main()
