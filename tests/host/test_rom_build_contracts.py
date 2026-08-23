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

    def test_font_build_chain_starts_at_both_localization_catalogs(self) -> None:
        makefile = (ROOT / "Makefile").read_text(encoding="utf-8")
        self.assertRegex(
            makefile,
            r"(?m)^FONT_CATALOGS\s*:=\s*"
            r"source/localization/strings_zh_cn\.c\s*\\\s*"
            r"source/localization/strings_en\.c$",
        )
        self.assertRegex(
            makefile,
            r"(?m)^\$\(FONT_GLYPHS\):\s+\$\(FONT_CATALOGS\) "
            r"tools/extract_glyphs\.py$",
        )
        self.assertRegex(
            makefile,
            r"(?m)^\$\(FONT_GENERATED_ASSETS\) &: "
            r"\$\(FONT_GLYPHS\) tools/build_font\.py$",
        )
        self.assertRegex(
            makefile,
            r"(?m)^\$\(FONT_RUNTIME_ASSETS\) &: "
            r"\$\(FONT_ATLAS\) \$\(FONT_METRICS\) "
            r"tools/convert_font_atlas\.py$",
        )
        self.assertRegex(
            makefile,
            r"(?m)^\$\(OBJS_SOURCES\):\s+\$\(FONT_METRICS\)$",
        )


if __name__ == "__main__":
    unittest.main()
