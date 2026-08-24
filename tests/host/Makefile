SHELL := powershell.exe
.SHELLFLAGS := -NoProfile -Command
HOST_TOOL_RESOLVER := resolve_host_tool.ps1
VCVARS ?= $(strip $(shell powershell.exe -NoProfile -ExecutionPolicy Bypass -File "$(HOST_TOOL_RESOLVER)" -Tool VcVars))
PYTHON ?= $(strip $(shell powershell.exe -NoProfile -ExecutionPolicy Bypass -File "$(HOST_TOOL_RESOLVER)" -Tool Python))

ifeq ($(strip $(VCVARS)),)
$(error Unable to discover cl.exe or vcvars64.bat; set VCVARS on the command line)
endif
ifeq ($(strip $(PYTHON)),)
$(error Unable to discover Python 3; set PYTHON on the command line)
endif

MSVC_SETUP = $(if $(filter __PATH__,$(VCVARS)),,call "$(VCVARS)" >nul && )

TESTS := test_config.exe test_localization.exe test_audio_service.exe test_music_stream.exe test_texture_ids.exe test_texture_cache.exe test_font_layout.exe test_scene_init.exe test_title_scene_draw.exe test_battle.exe

.PHONY: all clean host-test test_localization test_battle

all: host-test

host-test: $(TESTS)
	./test_config.exe
	./test_localization.exe
	./test_audio_service.exe
	./test_music_stream.exe
	./test_texture_ids.exe
	./test_texture_cache.exe
	./test_font_layout.exe
	./test_scene_init.exe
	./test_title_scene_draw.exe
	./test_battle.exe
	& "$(PYTHON)" test_font_assets.py
	& "$(PYTHON)" test_font_runtime_assets.py
	& "$(PYTHON)" test_asset_manifest.py
	& "$(PYTHON)" test_asset_dependencies.py
	& "$(PYTHON)" test_rom_build_contracts.py
	powershell.exe -NoProfile -ExecutionPolicy Bypass -File test_host_tool_discovery.ps1
	powershell.exe -NoProfile -ExecutionPolicy Bypass -File test_audio_assets.ps1

test_localization: test_localization.exe
	./test_localization.exe

test_config.exe: test_config.c ../../include/game_config.h
	$$env:TEMP = (Get-Location).Path; $$env:TMP = $$env:TEMP; cmd.exe /c '$(MSVC_SETUP)cl.exe /nologo /std:c11 /W4 /WX /I../../include $< /Fe:$@'

test_localization.exe: test_localization.c ../../include/game_terms.h ../../include/localization.h ../../source/localization/localization.c ../../source/localization/strings_zh_cn.c ../../source/localization/strings_en.c
	$$env:TEMP = (Get-Location).Path; $$env:TMP = $$env:TEMP; cmd.exe /c '$(MSVC_SETUP)cl.exe /nologo /std:c11 /utf-8 /W4 /WX /I../../include test_localization.c ../../source/localization/localization.c ../../source/localization/strings_zh_cn.c ../../source/localization/strings_en.c /Fe:$@'

test_audio_service.exe: test_audio_service.c ../../include/audio_service.h ../../source/audio/audio_service.c stubs/filesystem.h stubs/maxmod9.h stubs/nds.h stubs/soundbank.h
	$$env:TEMP = (Get-Location).Path; $$env:TMP = $$env:TEMP; cmd.exe /c '$(MSVC_SETUP)cl.exe /nologo /std:c11 /W4 /WX /Drand=testRand /Istubs /I../../include test_audio_service.c ../../source/audio/audio_service.c /Fe:$@'

test_music_stream.exe: test_music_stream.c ../../include/audio_service.h ../../source/audio/music_stream.c stubs/maxmod9.h stubs/nds.h
	$$env:TEMP = (Get-Location).Path; $$env:TMP = $$env:TEMP; cmd.exe /c '$(MSVC_SETUP)cl.exe /nologo /std:c11 /W4 /WX /D_CRT_SECURE_NO_WARNINGS /Dfopen=testFopen /Istubs /I../../include test_music_stream.c ../../source/audio/music_stream.c /Fe:$@'

test_texture_ids.exe: test_texture_ids.c ../../include/graphics_service.h ../../include/font_layout.h ../../include/generated/jimidou_font_metrics.h ../../include/game_config.h
	$$env:TEMP = (Get-Location).Path; $$env:TMP = $$env:TEMP; cmd.exe /c '$(MSVC_SETUP)cl.exe /nologo /std:c11 /W4 /WX /I../../include $< /Fe:$@'

test_texture_cache.exe: test_texture_cache.c ../../include/graphics_service.h ../../source/graphics/cat_textures.c stubs/gl2d.h
	$$env:TEMP = (Get-Location).Path; $$env:TMP = $$env:TEMP; cmd.exe /c '$(MSVC_SETUP)cl.exe /nologo /std:c11 /W4 /WX /Dfopen=testFopen /Istubs /I../../include test_texture_cache.c ../../source/graphics/cat_textures.c /Fe:$@'

test_font_layout.exe: test_font_layout.c ../../include/font_layout.h ../../include/generated/jimidou_font_metrics.h ../../include/localization.h ../../source/graphics/font_layout.c ../../source/localization/localization.c ../../source/localization/strings_zh_cn.c ../../source/localization/strings_en.c
	$$env:TEMP = (Get-Location).Path; $$env:TMP = $$env:TEMP; cmd.exe /c '$(MSVC_SETUP)cl.exe /nologo /std:c11 /utf-8 /W4 /WX /I../../include test_font_layout.c ../../source/graphics/font_layout.c ../../source/localization/localization.c ../../source/localization/strings_zh_cn.c ../../source/localization/strings_en.c /Fe:$@'

test_scene_init.exe: test_scene_init.c ../../include/ui_scene.h
	$$env:TEMP = (Get-Location).Path; $$env:TMP = $$env:TEMP; cmd.exe /c '$(MSVC_SETUP)cl.exe /nologo /std:c11 /W4 /WX /I../../include test_scene_init.c ../../source/ui/scene_status.c /Fe:$@'

test_title_scene_draw.exe: test_title_scene_draw.c ../../include/ui_scene.h ../../include/graphics_service.h ../../include/localization.h ../../source/ui/title_scene.c ../../source/ui/scene_status.c stubs/nds.h
	$$env:TEMP = (Get-Location).Path; $$env:TMP = $$env:TEMP; cmd.exe /c '$(MSVC_SETUP)cl.exe /nologo /std:c11 /W4 /WX /Istubs /I../../include test_title_scene_draw.c ../../source/ui/title_scene.c ../../source/ui/scene_status.c /Fe:$@'

test_battle.exe: test_battle.c ../../include/battle.h ../../include/battle_rng.h ../../source/battle/battle_core.c ../../source/battle/battle_rng.c
	$$env:TEMP = (Get-Location).Path; $$env:TMP = $$env:TEMP; cmd.exe /c '$(MSVC_SETUP)cl.exe /nologo /std:c11 /W4 /WX /I../../include test_battle.c ../../source/battle/battle_core.c ../../source/battle/battle_rng.c /Fe:$@'

test_battle: test_battle.exe
	./test_battle.exe

clean:
	Remove-Item -Force -ErrorAction SilentlyContinue $(TESTS)
