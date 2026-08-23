SHELL := powershell.exe
.SHELLFLAGS := -NoProfile -Command
VCVARS := C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat

TESTS := test_config.exe

.PHONY: all clean host-test

all: host-test

host-test: $(TESTS)
	./test_config.exe

test_config.exe: test_config.c ../../include/game_config.h
	$$env:TEMP = (Get-Location).Path; $$env:TMP = $$env:TEMP; cmd.exe /c 'call "$(VCVARS)" >nul && cl.exe /nologo /std:c11 /W4 /WX /I../../include $< /Fe:$@'

clean:
	Remove-Item -Force -ErrorAction SilentlyContinue $(TESTS)
