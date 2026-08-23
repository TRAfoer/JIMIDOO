# Task 3 — Deterministic Cat Image Pipeline Report

## Result

The repository now contains the 35 immutable RGBA source cutouts, 35 deterministic
128×128 RGBA review PNGs, matching `.grit` option files, and 35 pairs of NitroFS
texture/palette binaries. `assets/cats` remains outside `GFXDIRS`; no cat data is
linked into the ARM9 ELF. `nitrofs/cats` is packaged by the existing NitroFS build
path for Task 5 to load at runtime.

`ASSET_PYTHON` used for generation and tests was
`C:\Users\lhf19\AppData\Local\Programs\Python\Python312\python.exe`
(Python 3.12.6, Pillow 11.2.1). `tools/requirements-assets.txt` pins that Pillow
version for repeatable generation.

## RED/GREEN evidence

1. RED: `tests/host/test_asset_manifest.py` was written first. Running
   `C:\Users\lhf19\AppData\Local\Programs\Python\Python312\python.exe -m unittest tests.host.test_asset_manifest -v`
   initially failed with `FileNotFoundError` for `tools/cat_manifest.json`.
2. RED: after adding only the manifest, the same command failed as intended: the
   expected 35 named PNG stems were absent from `assets/cats` (zero generated
   PNGs).
3. GREEN: after implementation and generation, that command passed. The test
   asserts exact source mappings, exact generated stem set, 128×128 RGBA/nonempty
   PNGs, 16,384-byte indexed texture images, palette sizes within 2–512 bytes and
   even, and that every texture index is inside its palette.
4. Determinism: hashes for all 35 processed PNGs, 35 `.grit` files, and 70 NitroFS
   binaries were taken before and after a second `process_cats.py` run; PowerShell
   `Compare-Object` produced no differences.

## Source provenance and fetch behavior

`tools/cat_manifest.json` records the specified remote source template:

```text
https://www.bilibilitoy.com/toy/YmMvNRJDNp6gRKsk/24126970560512-v8847/Image/cat_{cat}_{action}.png
```

The exact mappings are `a,b,c,d,e → orange,tabby,chouju,maodie,banana` and
`1..7 → yowl,hiss,scratch,hit,heal,dead,idle`. The pre-verified local cache at
`C:\Users\lhf19\AppData\Local\Temp\jimidou-cat-matrix` supplied the 35
`cat_<letter>_<number>.png` source files; it was only read, never changed. All 35
files are copied into `assets_src/cats`, so the project does not rely on that temp
directory after this task.

`fetch_cats.py --output assets_src/cats` was exercised with the generated source
directory. It reported `keep` for each of the 35 existing source files and made no
network request. For a missing file it uses the manifest URL, requires HTTP 200 and
the PNG signature, then writes `<name>.png.part` and atomically renames it only
after validation.

## Runtime binary contract

For every processed `<stem>.png` there are these NitroFS files:

```text
nitrofs/cats/<stem>.img.bin  # 128 * 128 = 16,384 row-major 8-bit indices
nitrofs/cats/<stem>.pal.bin  # 256 little-endian Nintendo DS BGR555 entries = 512 bytes
```

Index 0 is reserved for the `FF00FF` transparency key. The other indices are a
deterministic no-dither, median-cut 255-color quantization of the 128×128 review
PNG. The adjacent `<stem>.grit` records the matching GL source settings:
`-g -gTFF00FF -gt -gB8 -m! -p!`. The Python encoder is the deterministic
equivalent used to emit the external index and palette files required by the
NitroFS loading contract; Task 5 must use row-major 8-bit indices and the paired
BGR555 palette, treating index 0 as transparent.

## Visual QA

A native DS-scale 256×192 contact sheet was created at
`build/cat-contact-sheet.png` and inspected. It shows all 35 actions on a dark
background: no opaque white boxes are visible, and the cutouts retain visible paws,
tails, ears, and body extents after centering/scaling. The QA file is a disposable,
ignored build artifact.

## Verification commands and results

Focused generation and artifact check:

```powershell
& $assetPython tools\fetch_cats.py --output assets_src\cats
& $assetPython tools\process_cats.py
& $assetPython -m unittest tests.host.test_asset_manifest -v
```

Result: PASS, 35 source PNGs, 35 review PNGs, 35 `.grit` files, 35 image binaries,
and 35 palette binaries.

Full host suite:

```text
C:\msys64\usr\bin\bash.exe -lc "cd /c/Desktop/JiMiDoo/.worktrees/foundation-assets && make host-test"
```

Result: PASS: configuration, localization, font-artifact, and cat-asset tests.

Forced ARM9/NitroFS ROM build:

```text
C:\msys64\usr\bin\bash.exe -lc 'export PATH=/opt/wonderful/bin:$PATH; export BLOCKSDS=/opt/wonderful/thirdparty/blocksds/core; cd /c/Desktop/JiMiDoo/.worktrees/foundation-assets && mkdir -p build/tmp && export TMP="$PWD/build/tmp" TEMP="$PWD/build/tmp" && make clean && mkdir -p build/tmp && make -j2'
```

Result: PASS, compiling ARM9 sources, linking `build/PussiFight.elf`, and producing
`PussiFight.nds` through `ndstool` with the `nitrofs` directory.

## Files delivered

- `tools/cat_manifest.json`, `tools/fetch_cats.py`, `tools/process_cats.py`, and
  `tools/requirements-assets.txt`
- `tests/host/test_asset_manifest.py` and the host-test Makefile hook
- `assets_src/cats/cat_{a..e}_{1..7}.png`
- `assets/cats/<stem>.png` and `<stem>.grit` (35 each)
- `nitrofs/cats/<stem>.img.bin` and `<stem>.pal.bin` (35 each)

## Concerns

The current MSYS sandbox does not expose BlocksDS at the Makefile default
`/opt/blocksds/core`, and its base PATH cannot load Wonderful GCC's dependent DLLs;
the compiler also cannot write the default `C:\msys64\tmp`. The forced ROM build
therefore needs the documented `PATH`, `BLOCKSDS`, `TMP`, and `TEMP` environment
overrides. This is environment setup, not a cat-pipeline defect.
