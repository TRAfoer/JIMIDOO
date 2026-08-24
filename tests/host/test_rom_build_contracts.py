"""Check release/build contracts that are independent of ARM execution."""

import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import time
import unittest


ROOT = Path(__file__).resolve().parents[2]
MAKE = shutil.which("make")


class RomBuildContractTest(unittest.TestCase):
    def run_make(self, *arguments: str) -> subprocess.CompletedProcess[str]:
        if MAKE is None:
            self.fail("make is required for build-contract tests")
        environment = os.environ.copy()
        for inherited in ("MAKEFLAGS", "MFLAGS", "MAKELEVEL"):
            environment.pop(inherited, None)
        return subprocess.run(
            [MAKE, "--no-print-directory", *arguments],
            cwd=ROOT,
            check=True,
            env=environment,
            text=True,
            capture_output=True,
        )

    def make_sandbox(self, temporary: Path) -> dict[str, str]:
        build_dir = temporary / "nds"
        info_dir = temporary / "maxmod"
        soundbank_dir = temporary / "maxmod_nitrofs"
        build_dir.mkdir()
        info_dir.mkdir()
        soundbank_dir.mkdir()

        controlled = {
            "ROM": temporary / "PussiFight.nds",
            "ELF": temporary / "PussiFight.elf",
            "BUILDDIR": build_dir,
            "SOUNDBANKINFODIR": info_dir,
            "SOUNDBANKDIR": soundbank_dir,
            "FONT_CATALOGS": [
                temporary / "strings_zh_cn.c",
                temporary / "strings_en.c",
            ],
            "FONT_GLYPHS": temporary / "required_glyphs.txt",
            "FONT_SUBSET": temporary / "jimidou_subset.ttf",
            "FONT_ATLAS": temporary / "jimidou_font_atlas.png",
            "FONT_METRICS": temporary / "jimidou_font_metrics.h",
            "FONT_RUNTIME_IMAGE": temporary / "jimidou_font.a5i3.bin",
            "FONT_RUNTIME_PALETTE": temporary / "jimidou_font.pal.bin",
            "NITROFS_CAT_PAYLOADS": temporary / "orange_idle.img.bin",
            "NITROFS_BGM_PAYLOADS": temporary / "menu.wav",
            "SOURCES_AUDIO": temporary / "start.wav",
        }
        controlled["ROM"].write_bytes(b"rom")
        controlled["ELF"].write_bytes(b"elf")
        (info_dir / "soundbank.h").write_bytes(b"header")
        (soundbank_dir / "soundbank.bin").write_bytes(b"bank")
        for name, paths in controlled.items():
            if name in {"ROM", "ELF", "BUILDDIR", "SOUNDBANKINFODIR", "SOUNDBANKDIR"}:
                continue
            for path in paths if isinstance(paths, list) else [paths]:
                path.write_bytes(b"asset")

        future = time.time() + 86400
        timestamp_groups = (
            controlled["FONT_CATALOGS"],
            [controlled["FONT_GLYPHS"]],
            [
                controlled["FONT_SUBSET"],
                controlled["FONT_ATLAS"],
                controlled["FONT_METRICS"],
            ],
            [
                controlled["FONT_RUNTIME_IMAGE"],
                controlled["FONT_RUNTIME_PALETTE"],
                controlled["NITROFS_CAT_PAYLOADS"],
                controlled["NITROFS_BGM_PAYLOADS"],
                controlled["SOURCES_AUDIO"],
                info_dir / "soundbank.h",
                soundbank_dir / "soundbank.bin",
                controlled["ELF"],
            ],
            [controlled["ROM"]],
        )
        for index, paths in enumerate(timestamp_groups):
            timestamp = future + index
            for path in paths:
                os.utime(path, (timestamp, timestamp))

        return {
            name: " ".join(path.relative_to(ROOT).as_posix() for path in paths)
            if isinstance(paths, list)
            else paths.relative_to(ROOT).as_posix()
            for name, paths in controlled.items()
        }

    def test_default_goal_builds_the_rom(self) -> None:
        """Bare make must traverse source compilation through ROM packaging."""
        (ROOT / "build").mkdir(exist_ok=True)
        with tempfile.TemporaryDirectory(dir=ROOT / "build") as directory:
            variables = self.make_sandbox(Path(directory))
            result = self.run_make(
                "--dry-run",
                "--always-make",
                *(f"{name}={value}" for name, value in variables.items()),
            )

            self.assertIn("NDSTOOL", result.stdout)

    def test_payload_changes_schedule_their_assets_and_rom(self) -> None:
        """Dropping any direct payload edge must suppress its expected rebuild."""
        (ROOT / "build").mkdir(exist_ok=True)
        with tempfile.TemporaryDirectory(dir=ROOT / "build") as directory:
            variables = self.make_sandbox(Path(directory))
            common = [f"{name}={value}" for name, value in variables.items()]
            common.append(f"--old-file={variables['ELF']}")
            cases = (
                (variables["NITROFS_CAT_PAYLOADS"], False),
                (variables["NITROFS_BGM_PAYLOADS"], False),
                (variables["FONT_RUNTIME_IMAGE"], False),
            )
            for changed, expects_soundbank in cases:
                with self.subTest(changed=changed):
                    result = self.run_make(
                        "--dry-run",
                        f"--what-if={changed}",
                        *common,
                        variables["ROM"],
                    )
                    self.assertIn("NDSTOOL", result.stdout)
                    self.assertEqual("MMUTIL" in result.stdout, expects_soundbank)
                    self.assertNotIn("GLYPHS", result.stdout)
                    self.assertNotIn("FONT    ", result.stdout)
                    self.assertNotIn("FONT.NDS", result.stdout)

    def test_direct_payload_ignores_a_stale_live_font_catalog(self) -> None:
        """A cat payload must not traverse the live font dependency chain."""
        (ROOT / "build").mkdir(exist_ok=True)
        with tempfile.TemporaryDirectory(dir=ROOT / "build") as directory:
            variables = self.make_sandbox(Path(directory))
            cat_payload_path = variables["NITROFS_CAT_PAYLOADS"]
            common = [f"{name}={value}" for name, value in variables.items()]
            common.append(f"--old-file={variables['ELF']}")

            leaked_common = [
                argument for argument in common if not argument.startswith("FONT_")
            ]
            leaked_result = self.run_make(
                "--dry-run",
                "--what-if=source/localization/strings_zh_cn.c",
                f"--what-if={cat_payload_path}",
                *leaked_common,
                variables["ROM"],
            )
            self.assertIn("GLYPHS", leaked_result.stdout)

            result = self.run_make(
                "--dry-run",
                "--what-if=source/localization/strings_zh_cn.c",
                f"--what-if={cat_payload_path}",
                *common,
                variables["ROM"],
            )

            self.assertIn("NDSTOOL", result.stdout)
            self.assertNotIn("GLYPHS", result.stdout)
            self.assertNotIn("FONT    ", result.stdout)
            self.assertNotIn("FONT.NDS", result.stdout)

    def test_sfx_change_regenerates_soundbank_and_rom(self) -> None:
        """An SFX update must flow through mmutil into a newly packed ROM."""
        (ROOT / "build").mkdir(exist_ok=True)
        with tempfile.TemporaryDirectory(dir=ROOT / "build") as directory:
            temporary = Path(directory)
            variables = self.make_sandbox(temporary)
            fake_blocksds = temporary / "blocksds"
            fake_mmutil = fake_blocksds / "tools" / "mmutil" / "mmutil"
            fake_ndstool = fake_blocksds / "tools" / "ndstool" / "ndstool"
            fake_mmutil.parent.mkdir(parents=True)
            fake_ndstool.parent.mkdir(parents=True)
            future_marker = temporary / "future"
            mmutil_log = temporary / "mmutil.log"
            ndstool_log = temporary / "ndstool.log"
            future_marker.write_bytes(b"future")
            future = time.time() + 172800
            os.utime(future_marker, (future, future))

            marker_path = future_marker.relative_to(ROOT).as_posix()
            mmutil_log_path = mmutil_log.relative_to(ROOT).as_posix()
            ndstool_log_path = ndstool_log.relative_to(ROOT).as_posix()
            fake_mmutil.write_text(
                "#!/usr/bin/env sh\n"
                "set -eu\n"
                "output=\n"
                "header=\n"
                "for argument in \"$@\"; do\n"
                "  case \"$argument\" in\n"
                "    -o*) output=${argument#-o} ;;\n"
                "    -h*) header=${argument#-h} ;;\n"
                "  esac\n"
                "done\n"
                "mkdir -p \"$(dirname \"$output\")\" \"$(dirname \"$header\")\"\n"
                ": > \"$output\"\n"
                ": > \"$header\"\n"
                f"touch -r '{marker_path}' \"$output\" \"$header\"\n"
                f"printf 'run\\n' >> '{mmutil_log_path}'\n",
                encoding="utf-8",
                newline="\n",
            )
            fake_ndstool.write_text(
                "#!/usr/bin/env sh\n"
                "set -eu\n"
                "output=\n"
                "while [ \"$#\" -gt 0 ]; do\n"
                "  if [ \"$1\" = '-c' ]; then output=$2; break; fi\n"
                "  shift\n"
                "done\n"
                ": > \"$output\"\n"
                f"printf 'run\\n' >> '{ndstool_log_path}'\n",
                encoding="utf-8",
                newline="\n",
            )
            fake_mmutil.chmod(0o755)
            fake_ndstool.chmod(0o755)

            common = [f"{name}={value}" for name, value in variables.items()]
            result = self.run_make(
                f"--what-if={variables['SOURCES_AUDIO']}",
                f"BLOCKSDS={fake_blocksds.relative_to(ROOT).as_posix()}",
                f"--old-file={variables['ELF']}",
                *common,
                variables["ROM"],
            )
            self.assertEqual(mmutil_log.read_text(encoding="utf-8"), "run\n")
            self.assertEqual(ndstool_log.read_text(encoding="utf-8"), "run\n")
            self.assertIn("MMUTIL", result.stdout)
            self.assertIn("NDSTOOL", result.stdout)
            self.assertNotIn("GLYPHS", result.stdout)
            self.assertNotIn("FONT    ", result.stdout)
            self.assertNotIn("FONT.NDS", result.stdout)

    def test_catalog_change_regenerates_font_payload_and_rom(self) -> None:
        """A catalog update must flow through glyph, font, and ROM generation."""
        (ROOT / "build").mkdir(exist_ok=True)
        with tempfile.TemporaryDirectory(dir=ROOT / "build") as directory:
            temporary = Path(directory)
            variables = self.make_sandbox(temporary)
            font_paths = {
                "FONT_CATALOGS": [
                    ROOT / path for path in variables["FONT_CATALOGS"].split()
                ],
                "FONT_GLYPHS": [ROOT / variables["FONT_GLYPHS"]],
                "FONT_SUBSET": [ROOT / variables["FONT_SUBSET"]],
                "FONT_ATLAS": [ROOT / variables["FONT_ATLAS"]],
                "FONT_METRICS": [ROOT / variables["FONT_METRICS"]],
                "FONT_RUNTIME_IMAGE": [ROOT / variables["FONT_RUNTIME_IMAGE"]],
                "FONT_RUNTIME_PALETTE": [ROOT / variables["FONT_RUNTIME_PALETTE"]],
            }

            base_time = time.time() + 172800
            ordered = (
                [Path(variables["ELF"]),
                 Path(variables["SOUNDBANKDIR"]) / "soundbank.bin"]
                + [Path(variables["ROM"])]
                + font_paths["FONT_RUNTIME_IMAGE"]
                + font_paths["FONT_RUNTIME_PALETTE"]
                + font_paths["FONT_SUBSET"]
                + font_paths["FONT_ATLAS"]
                + font_paths["FONT_METRICS"]
                + font_paths["FONT_GLYPHS"]
                + font_paths["FONT_CATALOGS"]
            )
            for index, path in enumerate(ordered):
                absolute = path if path.is_absolute() else ROOT / path
                timestamp = base_time + index
                os.utime(absolute, (timestamp, timestamp))

            common = [f"{name}={value}" for name, value in variables.items()]
            result = self.run_make(
                "--dry-run",
                f"--old-file={variables['ELF']}",
                *common,
                variables["ROM"],
            )
            for expected_step in ("GLYPHS", "FONT    ", "FONT.NDS", "NDSTOOL"):
                self.assertIn(expected_step, result.stdout)

    def test_missing_soundbank_binary_regenerates_the_grouped_outputs(self) -> None:
        """Deleting only soundbank.bin must rerun mmutil even if its header exists."""
        (ROOT / "build").mkdir(exist_ok=True)
        with tempfile.TemporaryDirectory(dir=ROOT / "build") as directory:
            temporary = Path(directory)
            fake_blocksds = temporary / "blocksds"
            fake_mmutil = fake_blocksds / "tools" / "mmutil" / "mmutil"
            fake_mmutil.parent.mkdir(parents=True)
            invocation_log = temporary / "mmutil.log"
            log_path = invocation_log.relative_to(ROOT).as_posix()
            fake_mmutil.write_text(
                "#!/usr/bin/env sh\n"
                "set -eu\n"
                "output=\n"
                "header=\n"
                "for argument in \"$@\"; do\n"
                "  case \"$argument\" in\n"
                "    -o*) output=${argument#-o} ;;\n"
                "    -h*) header=${argument#-h} ;;\n"
                "  esac\n"
                "done\n"
                "mkdir -p \"$(dirname \"$output\")\" \"$(dirname \"$header\")\"\n"
                ": > \"$output\"\n"
                ": > \"$header\"\n"
                f"printf 'run\\n' >> '{log_path}'\n",
                encoding="utf-8",
                newline="\n",
            )
            fake_mmutil.chmod(0o755)

            info_dir = temporary / "maxmod"
            soundbank_dir = temporary / "maxmod_nitrofs"
            binary = soundbank_dir / "soundbank.bin"
            header = info_dir / "soundbank.h"
            variables = (
                f"BLOCKSDS={fake_blocksds.relative_to(ROOT).as_posix()}",
                f"SOUNDBANKINFODIR={info_dir.relative_to(ROOT).as_posix()}",
                f"SOUNDBANKDIR={soundbank_dir.relative_to(ROOT).as_posix()}",
            )
            target = binary.relative_to(ROOT).as_posix()

            self.run_make(*variables, target)
            self.assertTrue(binary.is_file())
            self.assertTrue(header.is_file())
            binary.unlink()
            self.assertTrue(header.is_file())

            self.run_make(*variables, target)
            self.assertTrue(binary.is_file())
            self.assertTrue(header.is_file())
            self.assertEqual(invocation_log.read_text(encoding="utf-8"), "run\nrun\n")

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
