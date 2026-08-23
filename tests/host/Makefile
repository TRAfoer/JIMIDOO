SHELL := powershell.exe
.SHELLFLAGS := -NoProfile -Command
VCVARS := C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat
PYTHON ?= C:/Users/lhf19/AppData/Local/Programs/Python/Python312/python.exe

TESTS := test_config.exe test_localization.exe test_audio_service.exe test_music_stream.exe

.PHONY: all clean host-test test_localization

all: host-test

host-test: $(TESTS)
	./test_config.exe
	./test_localization.exe
	./test_audio_service.exe
	./test_music_stream.exe
	& "$(PYTHON)" test_font_assets.py
	& "$(PYTHON)" test_asset_manifest.py
	powershell.exe -NoProfile -ExecutionPolicy Bypass -File test_audio_assets.ps1

test_localization: test_localization.exe
	./test_localization.exe

test_config.exe: test_config.c ../../include/game_config.h
	$$env:TEMP = (Get-Location).Path; $$env:TMP = $$env:TEMP; cmd.exe /c 'call "$(VCVARS)" >nul && cl.exe /nologo /std:c11 /W4 /WX /I../../include $< /Fe:$@'

test_localization.exe: test_localization.c ../../include/game_terms.h ../../include/localization.h ../../source/localization/localization.c ../../source/localization/strings_zh_cn.c ../../source/localization/strings_en.c
	$$env:TEMP = (Get-Location).Path; $$env:TMP = $$env:TEMP; cmd.exe /c 'call "$(VCVARS)" >nul && cl.exe /nologo /std:c11 /utf-8 /W4 /WX /I../../include test_localization.c ../../source/localization/localization.c ../../source/localization/strings_zh_cn.c ../../source/localization/strings_en.c /Fe:$@'

test_audio_service.exe: test_audio_service.c ../../include/audio_service.h ../../source/audio/audio_service.c stubs/filesystem.h stubs/maxmod9.h stubs/nds.h stubs/soundbank.h
	$$env:TEMP = (Get-Location).Path; $$env:TMP = $$env:TEMP; cmd.exe /c 'call "$(VCVARS)" >nul && cl.exe /nologo /std:c11 /W4 /WX /Drand=testRand /Istubs /I../../include test_audio_service.c ../../source/audio/audio_service.c /Fe:$@'

test_music_stream.exe: test_music_stream.c ../../include/audio_service.h ../../source/audio/music_stream.c stubs/maxmod9.h stubs/nds.h
	$$env:TEMP = (Get-Location).Path; $$env:TMP = $$env:TEMP; cmd.exe /c 'call "$(VCVARS)" >nul && cl.exe /nologo /std:c11 /W4 /WX /D_CRT_SECURE_NO_WARNINGS /Dfopen=testFopen /Istubs /I../../include test_music_stream.c ../../source/audio/music_stream.c /Fe:$@'

clean:
	Remove-Item -Force -ErrorAction SilentlyContinue $(TESTS)
