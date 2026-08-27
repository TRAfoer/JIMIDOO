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

TESTS := test_config.exe test_localization.exe test_audio_service.exe test_music_stream.exe test_texture_ids.exe test_texture_cache.exe test_battle_background.exe test_font_layout.exe test_scene_init.exe test_title_scene_draw.exe test_debug_scene.exe test_debug_scene_draw.exe test_battle.exe test_ai.exe test_ai_policy.exe test_ai_brain.exe test_touch_layout.exe test_battle_scene_policy.exe test_battle_animation.exe

.PHONY: all clean host-test test_localization test_battle test_ai test_ai_brain test_touch_layout test_battle_scene_policy test_battle_animation

all: host-test

host-test: $(TESTS)
	./test_config.exe
	./test_localization.exe
	./test_audio_service.exe
	./test_music_stream.exe
	./test_texture_ids.exe
	./test_texture_cache.exe
	./test_battle_background.exe
	./test_font_layout.exe
	./test_scene_init.exe
	./test_title_scene_draw.exe
	./test_debug_scene.exe
	./test_debug_scene_draw.exe
	./test_battle.exe
	./test_ai.exe
	./test_ai_policy.exe
	./test_ai_brain.exe
	./test_touch_layout.exe
	./test_battle_scene_policy.exe
	./test_battle_animation.exe
	& "$(PYTHON)" test_font_assets.py
	& "$(PYTHON)" test_font_runtime_assets.py
	& "$(PYTHON)" test_asset_manifest.py
	& "$(PYTHON)" test_battle_background_assets.py
	& "$(PYTHON)" test_battle_render_contract.py
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

test_battle_background.exe: test_battle_background.c ../../source/graphics/battle_background.c stubs/gl2d.h
	$$env:TEMP = (Get-Location).Path; $$env:TMP = $$env:TEMP; cmd.exe /c '$(MSVC_SETUP)cl.exe /nologo /std:c11 /W4 /WX /Dfopen=testFopen /Istubs /I../../include test_battle_background.c ../../source/graphics/battle_background.c /Fe:$@'

test_font_layout.exe: test_font_layout.c ../../include/font_layout.h ../../include/generated/jimidou_font_metrics.h ../../include/localization.h ../../source/graphics/font_layout.c ../../source/localization/localization.c ../../source/localization/strings_zh_cn.c ../../source/localization/strings_en.c
	$$env:TEMP = (Get-Location).Path; $$env:TMP = $$env:TEMP; cmd.exe /c '$(MSVC_SETUP)cl.exe /nologo /std:c11 /utf-8 /W4 /WX /I../../include test_font_layout.c ../../source/graphics/font_layout.c ../../source/localization/localization.c ../../source/localization/strings_zh_cn.c ../../source/localization/strings_en.c /Fe:$@'

test_scene_init.exe: test_scene_init.c ../../include/ui_scene.h
	$$env:TEMP = (Get-Location).Path; $$env:TMP = $$env:TEMP; cmd.exe /c '$(MSVC_SETUP)cl.exe /nologo /std:c11 /W4 /WX /I../../include test_scene_init.c ../../source/ui/scene_status.c /Fe:$@'

test_title_scene_draw.exe: test_title_scene_draw.c ../../include/ui_scene.h ../../include/graphics_service.h ../../include/localization.h ../../source/ui/title_scene.c ../../source/ui/scene_status.c stubs/nds.h
	$$env:TEMP = (Get-Location).Path; $$env:TMP = $$env:TEMP; cmd.exe /c '$(MSVC_SETUP)cl.exe /nologo /std:c11 /W4 /WX /Istubs /I../../include test_title_scene_draw.c ../../source/ui/title_scene.c ../../source/ui/scene_status.c /Fe:$@'

test_debug_scene.exe: test_debug_scene.c ../../include/debug_scene.h ../../source/ui/debug_scene.c stubs/nds.h
	$$env:TEMP = (Get-Location).Path; $$env:TMP = $$env:TEMP; cmd.exe /c '$(MSVC_SETUP)cl.exe /nologo /std:c11 /W4 /WX /Istubs /I../../include test_debug_scene.c ../../source/ui/debug_scene.c /Fe:$@'

test_debug_scene_draw.exe: test_debug_scene_draw.c ../../include/debug_scene.h ../../include/graphics_service.h ../../include/localization.h ../../source/ui/debug_scene_view.c ../../source/ai/ai.c stubs/nds.h
	$$env:TEMP = (Get-Location).Path; $$env:TMP = $$env:TEMP; cmd.exe /c '$(MSVC_SETUP)cl.exe /nologo /std:c11 /W4 /WX /Istubs /I../../include test_debug_scene_draw.c ../../source/ui/debug_scene_view.c ../../source/ai/ai.c /Fe:$@'

test_battle.exe: test_battle.c ../../include/battle.h ../../include/battle_rng.h ../../source/battle/battle_core.c ../../source/battle/battle_rng.c
	$$env:TEMP = (Get-Location).Path; $$env:TMP = $$env:TEMP; cmd.exe /c '$(MSVC_SETUP)cl.exe /nologo /std:c11 /W4 /WX /I../../include test_battle.c ../../source/battle/battle_core.c ../../source/battle/battle_rng.c /Fe:$@'

test_battle: test_battle.exe
	./test_battle.exe

test_ai.exe: test_ai.c ../../include/ai.h ../../include/ai_internal.h ../../include/battle.h ../../include/battle_rng.h ../../include/game_config.h ../../source/ai/ai.c ../../source/ai/ai_policy.c ../../source/battle/battle_core.c ../../source/battle/battle_rng.c
	$$env:TEMP = (Get-Location).Path; $$env:TMP = $$env:TEMP; cmd.exe /c '$(MSVC_SETUP)cl.exe /nologo /std:c11 /W4 /WX /I../../include test_ai.c ../../source/ai/ai.c ../../source/ai/ai_policy.c ../../source/battle/battle_core.c ../../source/battle/battle_rng.c /Fe:$@'

test_ai: test_ai.exe
	./test_ai.exe

test_ai_policy.exe: test_ai_policy.c ../../include/ai.h ../../include/ai_internal.h ../../source/ai/ai.c ../../source/ai/ai_policy.c ../../source/battle/battle_core.c ../../source/battle/battle_rng.c
	$$env:TEMP = (Get-Location).Path; $$env:TMP = $$env:TEMP; cmd.exe /c '$(MSVC_SETUP)cl.exe /nologo /std:c11 /W4 /WX /I../../include test_ai_policy.c ../../source/ai/ai.c ../../source/ai/ai_policy.c ../../source/battle/battle_core.c ../../source/battle/battle_rng.c /Fe:$@'

test_ai_brain.exe: test_ai_brain.c ../../include/ai.h ../../source/ai/ai.c ../../source/ai/ai_policy.c ../../source/ai/ai_brain.c ../../source/battle/battle_core.c ../../source/battle/battle_rng.c
	$$env:TEMP = (Get-Location).Path; $$env:TMP = $$env:TEMP; cmd.exe /c '$(MSVC_SETUP)cl.exe /nologo /std:c11 /W4 /WX /I../../include test_ai_brain.c ../../source/ai/ai.c ../../source/ai/ai_policy.c ../../source/ai/ai_brain.c ../../source/battle/battle_core.c ../../source/battle/battle_rng.c /Fe:$@'

test_ai_brain: test_ai_brain.exe
	./test_ai_brain.exe

test_touch_layout.exe: test_touch_layout.c ../../include/battle_scene.h ../../include/battle_scene_internal.h ../../include/battle.h ../../source/ui/battle_scene.c ../../source/ui/battle_hud.c
	$$env:TEMP = (Get-Location).Path; $$env:TMP = $$env:TEMP; cmd.exe /c '$(MSVC_SETUP)cl.exe /nologo /std:c11 /W4 /WX /I../../include test_touch_layout.c ../../source/ui/battle_scene.c ../../source/ui/battle_hud.c /Fe:$@'

test_touch_layout: test_touch_layout.exe
	./test_touch_layout.exe

test_battle_scene_policy.exe: test_battle_scene_policy.c ../../include/battle_scene_internal.h ../../include/battle.h ../../source/ui/battle_scene.c
	$$env:TEMP = (Get-Location).Path; $$env:TMP = $$env:TEMP; cmd.exe /c '$(MSVC_SETUP)cl.exe /nologo /std:c11 /W4 /WX /I../../include test_battle_scene_policy.c ../../source/ui/battle_scene.c /Fe:$@'

test_battle_scene_policy: test_battle_scene_policy.exe
	./test_battle_scene_policy.exe

test_battle_animation.exe: test_battle_animation.c ../../include/battle_scene_internal.h ../../include/battle.h ../../source/graphics/battle_animation.c
	$$env:TEMP = (Get-Location).Path; $$env:TMP = $$env:TEMP; cmd.exe /c '$(MSVC_SETUP)cl.exe /nologo /std:c11 /W4 /WX /I../../include test_battle_animation.c ../../source/graphics/battle_animation.c /Fe:$@'

test_battle_animation: test_battle_animation.exe
	./test_battle_animation.exe

clean:
	Remove-Item -Force -ErrorAction SilentlyContinue $(TESTS)
