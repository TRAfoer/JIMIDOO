# SPDX-License-Identifier: CC0-1.0
#
# SPDX-FileContributor: Antonio Niño Díaz, 2023-2026

.DEFAULT_GOAL := all

export BLOCKSDS			?= /opt/blocksds/core
export BLOCKSDSEXT		?= /opt/blocksds/external

export WONDERFUL_TOOLCHAIN	?= /opt/wonderful
ARM_NONE_EABI_PATH	?= $(WONDERFUL_TOOLCHAIN)/toolchain/gcc-arm-none-eabi/bin/
PYTHON		?= python3
FONT_FILE	?=

# User config
# ===========

NAME		:= PussiFight

GAME_TITLE	:=
GAME_SUBTITLE	:=
GAME_AUTHOR	:=
GAME_ICON	:= icon.gif

# A compile_commands.json file is created if this is set to 1
COMPDB		:= 0

# DLDI and internal SD slot of DSi
# --------------------------------

# Root folder of the SD image
SDROOT		:= sdroot
# Name of the generated image it "DSi-1.sd" for no$gba in DSi mode
SDIMAGE		:= image.bin

# Source code paths
# -----------------

SOURCEDIRS	:= source
INCLUDEDIRS	:= include
GFXDIRS		:=
BINDIRS		:=
AUDIODIRS	:= assets/audio/sfx
# List of folders to combine into the root of NitroFS:
NITROFSDIR	:= nitrofs

FONT_CATALOGS	:= source/localization/strings_zh_cn.c \
		   source/localization/strings_en.c
DEFAULT_FONT_SOURCE	:= assets_src/fonts/FZG_CN.ttf
FONT_SOURCE	?= $(if $(strip $(FONT_FILE)),$(FONT_FILE),$(DEFAULT_FONT_SOURCE))
empty	:=
space	:= $(empty) $(empty)
FONT_SOURCE_PREREQUISITE	= $(subst $(space),\$(space),$(FONT_SOURCE))
FONT_GLYPHS	:= assets/fonts/required_glyphs.txt
FONT_SUBSET	:= assets/fonts/jimidou_subset.ttf
FONT_ATLAS	:= assets/fonts/jimidou_font_atlas.png
FONT_METRICS	:= include/generated/jimidou_font_metrics.h
FONT_GENERATED_ASSETS	:= $(FONT_SUBSET) $(FONT_ATLAS) $(FONT_METRICS)
FONT_RUNTIME_IMAGE	:= nitrofs/fonts/jimidou_font.a5i3.bin
FONT_RUNTIME_PALETTE	:= nitrofs/fonts/jimidou_font.pal.bin
FONT_RUNTIME_ASSETS	:= $(FONT_RUNTIME_IMAGE) $(FONT_RUNTIME_PALETTE)
FONT_FILE_ARGUMENT	= --font "$(FONT_SOURCE)"

# Defines passed to all files
# ---------------------------

DEFINES		:=

# Libraries
# ---------

# Remember to use an ARM7 core with dswifi if you use it on the ARM9
#ARM7ELF		:= $(BLOCKSDS)/sys/arm7/main_core/arm7_dswifi_maxmod.elf
ARM7ELF		:= $(BLOCKSDS)/sys/arm7/main_core/arm7_maxmod.elf

LIBS		:= -lmm9 -lnds9
LIBDIRS		:= $(BLOCKSDS)/libs/maxmod \
		   $(BLOCKSDS)/libs/libnds

# Build artifacts
# ---------------

BUILDDIR	:= build/$(NAME)
ELF		:= build/$(NAME).elf
DUMP		:= build/$(NAME).dump
MAP		:= build/$(NAME).map
ROM		:= $(NAME).nds

# If NITROFSDIR is set, the soundbank created by mmutil will be saved to NitroFS
SOUNDBANKINFODIR	:= $(BUILDDIR)/maxmod
ifeq ($(strip $(NITROFSDIR)),)
    SOUNDBANKDIR	:= $(BUILDDIR)/maxmod
else
    SOUNDBANKDIR	:= $(BUILDDIR)/maxmod_nitrofs
endif
SOUNDBANK_BINARY	:= $(SOUNDBANKDIR)/soundbank.bin

NITROFS_CAT_PAYLOADS	:= $(sort $(wildcard $(NITROFSDIR)/cats/*.img.bin) \
				   $(wildcard $(NITROFSDIR)/cats/*.pal.bin))
NITROFS_BACKGROUND_PAYLOADS := $(sort $(wildcard $(NITROFSDIR)/backgrounds/*.img.bin) \
					$(wildcard $(NITROFSDIR)/backgrounds/*.pal.bin))
NITROFS_BGM_PAYLOADS	:= $(sort $(wildcard $(NITROFSDIR)/audio/*.wav))
NITROFS_FONT_PAYLOADS	:= $(FONT_RUNTIME_ASSETS)
NITROFS_PAYLOADS	:= $(sort $(NITROFS_CAT_PAYLOADS) \
				   $(NITROFS_BACKGROUND_PAYLOADS) \
				   $(NITROFS_BGM_PAYLOADS) \
				   $(NITROFS_FONT_PAYLOADS))

# Tools
# -----

PREFIX		:= $(ARM_NONE_EABI_PATH)arm-none-eabi-
CC		:= $(PREFIX)gcc
CXX		:= $(PREFIX)g++
LD		:= $(PREFIX)gcc
OBJDUMP		:= $(PREFIX)objdump
MKDIR		:= mkdir
RM		:= rm -rf

# Verbose flag
# ------------

ifeq ($(VERBOSE),1)
V		:=
else
V		:= @
endif

# Source files
# ------------

ifneq ($(BINDIRS),)
    SOURCES_BIN	:= $(shell find -L $(BINDIRS) -name "*.bin")
    INCLUDEDIRS	+= $(addprefix $(BUILDDIR)/,$(BINDIRS))
endif
ifneq ($(GFXDIRS),)
    SOURCES_PNG	:= $(shell find -L $(GFXDIRS) -name "*.png")
    INCLUDEDIRS	+= $(addprefix $(BUILDDIR)/,$(GFXDIRS))
endif
ifneq ($(AUDIODIRS),)
    SOURCES_AUDIO	:= $(shell find -L $(AUDIODIRS) -regex '.*\.\(it\|mod\|s3m\|wav\|xm\)')
    ifneq ($(SOURCES_AUDIO),)
        INCLUDEDIRS	+= $(SOUNDBANKINFODIR)
    endif
endif

SOURCES_S	+= $(shell find -L $(SOURCEDIRS) -name "*.s")
SOURCES_C	+= $(shell find -L $(SOURCEDIRS) -name "*.c")
SOURCES_CPP	+= $(shell find -L $(SOURCEDIRS) -name "*.cpp")

# Compiler and linker flags
# -------------------------

DEFINES		+= -D__NDS__ -D__BLOCKSDS__ -DARM9

ARCH		:= -mthumb -mcpu=arm946e-s+nofp

SPECS		:= $(BLOCKSDS)/sys/crts/ds_arm9.specs

WARNFLAGS	:= -Wall

ifeq ($(SOURCES_CPP),)
	LIBS	+= -lc
else
	LIBS	+= -lstdc++ -lc
endif

INCLUDEFLAGS	:= $(foreach path,$(INCLUDEDIRS),-I$(path)) \
		   $(foreach path,$(LIBDIRS),-I$(path)/include)

LIBDIRSFLAGS	:= $(foreach path,$(LIBDIRS),-L$(path)/lib)

ASFLAGS		+= -x assembler-with-cpp $(INCLUDEFLAGS) $(DEFINES) \
		   $(ARCH) -ffunction-sections -fdata-sections \
		   -specs=$(SPECS)

CFLAGS		+= $(WARNFLAGS) $(INCLUDEFLAGS) $(DEFINES) \
		   $(ARCH) -O2 -ffunction-sections -fdata-sections \
		   -specs=$(SPECS)

CXXFLAGS	+= $(WARNFLAGS) $(INCLUDEFLAGS) $(DEFINES) \
		   $(ARCH) -O2 -ffunction-sections -fdata-sections \
		   -fno-exceptions -fno-rtti \
		   -specs=$(SPECS)

LDFLAGS		+= $(ARCH) $(LIBDIRSFLAGS) -Wl,-Map,$(MAP) $(DEFINES) \
		   -Wl,--start-group $(LIBS) -Wl,--end-group -specs=$(SPECS)

# Intermediate build files
# ------------------------

OBJS_ASSETS	:= $(addsuffix .o,$(addprefix $(BUILDDIR)/,$(SOURCES_BIN))) \
		   $(addsuffix .o,$(addprefix $(BUILDDIR)/,$(SOURCES_PNG)))

HEADERS_ASSETS	:= $(patsubst %.bin,%_bin.h,$(addprefix $(BUILDDIR)/,$(SOURCES_BIN))) \
		   $(patsubst %.png,%.h,$(addprefix $(BUILDDIR)/,$(SOURCES_PNG)))

ifneq ($(SOURCES_AUDIO),)
    ifeq ($(strip $(NITROFSDIR)),)
        OBJS_ASSETS		+= $(SOUNDBANKDIR)/soundbank.c.o
    endif
    HEADERS_ASSETS	+= $(SOUNDBANKINFODIR)/soundbank.h
endif

OBJS_SOURCES	:= $(addsuffix .o,$(addprefix $(BUILDDIR)/,$(SOURCES_S))) \
		   $(addsuffix .o,$(addprefix $(BUILDDIR)/,$(SOURCES_C))) \
		   $(addsuffix .o,$(addprefix $(BUILDDIR)/,$(SOURCES_CPP)))

$(OBJS_SOURCES): $(FONT_METRICS)

OBJS		:= $(OBJS_ASSETS) $(OBJS_SOURCES)

DEPS		:= $(OBJS:.o=.d)

# Targets
# -------

.PHONY: all asset-deps assets clean dump dldipatch font-runtime-assets host-test sdimage

all: $(ROM)

asset-deps:
	$(PYTHON) -m pip install --requirement tools/requirements-assets.txt

assets: font-runtime-assets

font-runtime-assets: $(FONT_RUNTIME_ASSETS)

$(ROM): $(FONT_RUNTIME_ASSETS)

ifneq ($(strip $(NITROFSDIR)),)
$(ROM): $(NITROFS_PAYLOADS)
ifneq ($(SOURCES_AUDIO),)
$(ROM): $(SOUNDBANK_BINARY)
endif
endif

$(FONT_GLYPHS): $(FONT_CATALOGS) tools/extract_glyphs.py
	@echo "  GLYPHS  $^"
	$(V)$(PYTHON) tools/extract_glyphs.py --output $(FONT_GLYPHS)

$(FONT_GENERATED_ASSETS) &: $(FONT_GLYPHS) $(FONT_SOURCE_PREREQUISITE) tools/build_font.py
	@echo "  FONT    $^"
	$(V)$(PYTHON) tools/build_font.py $(FONT_FILE_ARGUMENT) \
		--glyphs $(FONT_GLYPHS) \
		--output-font $(FONT_SUBSET) \
		--output-atlas $(FONT_ATLAS) \
		--output-metrics $(FONT_METRICS)

$(FONT_RUNTIME_ASSETS) &: $(FONT_ATLAS) $(FONT_METRICS) tools/convert_font_atlas.py
	@echo "  FONT.NDS $^"
	$(V)$(PYTHON) tools/convert_font_atlas.py \
		--input-atlas $(FONT_ATLAS) \
		--output-image $(FONT_RUNTIME_IMAGE) \
		--output-palette $(FONT_RUNTIME_PALETTE)

host-test:
	$(MAKE) -C tests/host

ifneq ($(strip $(NITROFSDIR)),)
# Additional arguments for ndstool
NDSTOOL_ARGS	:= -d $(NITROFSDIR)

ifneq ($(SOURCES_AUDIO),)
    NDSTOOL_ARGS	+= -d $(SOUNDBANKDIR)
endif

endif

# Combine the title strings
ifeq ($(strip $(GAME_TITLE)$(GAME_SUBTITLE)$(GAME_AUTHOR)),)
    GAME_FULL_TITLE :=
else ifeq ($(strip $(GAME_SUBTITLE)),)
    GAME_FULL_TITLE := $(GAME_TITLE);$(GAME_AUTHOR)
else
    GAME_FULL_TITLE := $(GAME_TITLE);$(GAME_SUBTITLE);$(GAME_AUTHOR)
endif

$(ROM): $(ELF)
	@echo "  NDSTOOL $@"
	$(V)$(BLOCKSDS)/tools/ndstool/ndstool -c $@ \
		-7 $(ARM7ELF) -9 $(ELF) \
		-b $(GAME_ICON) "$(GAME_FULL_TITLE)" \
		$(NDSTOOL_ARGS)

$(ELF): $(OBJS)
	@echo "  LD      $@"
	$(V)$(LD) -o $@ $(OBJS) $(LDFLAGS)

$(DUMP): $(ELF)
	@echo "  OBJDUMP   $@"
	$(V)$(OBJDUMP) -h -C -S $< > $@

dump: $(DUMP)

clean:
	@echo "  CLEAN"
	$(V)$(RM) $(ROM) $(DUMP) build $(SDIMAGE) compile_commands.json

sdimage:
	@echo "  MKFATIMG $(SDIMAGE) $(SDROOT)"
	$(V)$(BLOCKSDS)/tools/mkfatimg/mkfatimg -t $(SDROOT) $(SDIMAGE)

dldipatch: $(ROM)
	@echo "  DLDIPATCH $(ROM)"
	$(V)$(BLOCKSDS)/tools/dldipatch/dldipatch patch \
		$(BLOCKSDS)/sys/dldi_r4/r4tf.dldi $(ROM)

ifeq ($(COMPDB),1)
# Add an additional dependency to the "all" rule
all: compile_commands.json

compile_commands.json: $(OBJS)
	@echo "  MERGE   compile_commands.json"
	$(V)$(WONDERFUL_TOOLCHAIN)/bin/wf-compile-commands-merge $@ $(patsubst %.o,%.cc.json,$^)
endif

# Rules
# -----

ifeq ($(COMPDB),1)

$(BUILDDIR)/%.s.o : %.s
	@echo "  AS      $<"
	@$(MKDIR) -p $(@D)
	$(V)$(CC) $(ASFLAGS) -MMD -MP -c -MJ $(patsubst %.o,%.cc.json,$@) -o $@ $<

$(BUILDDIR)/%.c.o : %.c
	@echo "  CC      $<"
	@$(MKDIR) -p $(@D)
	$(V)$(CC) $(CFLAGS) -MMD -MP -c -MJ $(patsubst %.o,%.cc.json,$@) -o $@ $<

$(BUILDDIR)/%.arm.c.o : %.arm.c
	@echo "  CC      $<"
	@$(MKDIR) -p $(@D)
	$(V)$(CC) $(CFLAGS) -MMD -MP -marm -mlong-calls -c -MJ $(patsubst %.o,%.cc.json,$@) -o $@ $<

$(BUILDDIR)/%.cpp.o : %.cpp
	@echo "  CXX     $<"
	@$(MKDIR) -p $(@D)
	$(V)$(CXX) $(CXXFLAGS) -MMD -MP -c -MJ $(patsubst %.o,%.cc.json,$@) -o $@ $<

$(BUILDDIR)/%.arm.cpp.o : %.arm.cpp
	@echo "  CXX     $<"
	@$(MKDIR) -p $(@D)
	$(V)$(CXX) $(CXXFLAGS) -MMD -MP -marm -mlong-calls -c -MJ $(patsubst %.o,%.cc.json,$@) -o $@ $<

else

$(BUILDDIR)/%.s.o : %.s
	@echo "  AS      $<"
	@$(MKDIR) -p $(@D)
	$(V)$(CC) $(ASFLAGS) -MMD -MP -c -o $@ $<

$(BUILDDIR)/%.c.o : %.c
	@echo "  CC      $<"
	@$(MKDIR) -p $(@D)
	$(V)$(CC) $(CFLAGS) -MMD -MP -c -o $@ $<

$(BUILDDIR)/%.arm.c.o : %.arm.c
	@echo "  CC      $<"
	@$(MKDIR) -p $(@D)
	$(V)$(CC) $(CFLAGS) -MMD -MP -marm -mlong-calls -c -o $@ $<

$(BUILDDIR)/%.cpp.o : %.cpp
	@echo "  CXX     $<"
	@$(MKDIR) -p $(@D)
	$(V)$(CXX) $(CXXFLAGS) -MMD -MP -c -o $@ $<

$(BUILDDIR)/%.arm.cpp.o : %.arm.cpp
	@echo "  CXX     $<"
	@$(MKDIR) -p $(@D)
	$(V)$(CXX) $(CXXFLAGS) -MMD -MP -marm -mlong-calls -c -o $@ $<

endif

$(BUILDDIR)/%.bin.o $(BUILDDIR)/%_bin.h : %.bin
	@echo "  BIN2C   $<"
	@$(MKDIR) -p $(@D)
	$(V)$(BLOCKSDS)/tools/bin2c/bin2c $< $(@D)
	$(V)$(CC) $(CFLAGS) -MMD -MP -c -o $(BUILDDIR)/$*.bin.o $(BUILDDIR)/$*_bin.c

$(BUILDDIR)/%.png.o $(BUILDDIR)/%.h : %.png %.grit
	@echo "  GRIT    $<"
	@$(MKDIR) -p $(@D)
	$(V)$(BLOCKSDS)/tools/grit/grit $< -ftc -W1 -o$(BUILDDIR)/$*
	$(V)$(CC) $(CFLAGS) -MMD -MP -c -o $(BUILDDIR)/$*.png.o $(BUILDDIR)/$*.c
	$(V)touch $(BUILDDIR)/$*.png.o $(BUILDDIR)/$*.h

ifneq ($(SOURCES_AUDIO),)

$(SOUNDBANKINFODIR)/soundbank.h $(SOUNDBANK_BINARY) &: $(SOURCES_AUDIO)
	@echo "  MMUTIL  $^"
	@$(MKDIR) -p $(SOUNDBANKDIR)
	@$(MKDIR) -p $(SOUNDBANKINFODIR)
	$(V)$(BLOCKSDS)/tools/mmutil/mmutil $^ -d \
		-o$(SOUNDBANK_BINARY) -h$(SOUNDBANKINFODIR)/soundbank.h

ifeq ($(strip $(NITROFSDIR)),)
$(SOUNDBANKDIR)/soundbank.c.o: $(SOUNDBANKINFODIR)/soundbank.h
	@echo "  BIN2C   soundbank.bin"
	$(V)$(BLOCKSDS)/tools/bin2c/bin2c $(SOUNDBANKDIR)/soundbank.bin \
		$(SOUNDBANKDIR)
	@echo "  CC.9    soundbank_bin.c"
	$(V)$(CC) $(CFLAGS) -MMD -MP -c -o $(SOUNDBANKDIR)/soundbank.c.o \
		$(SOUNDBANKDIR)/soundbank_bin.c
endif

endif

# All assets must be built before the source code
# -----------------------------------------------

$(SOURCES_S) $(SOURCES_C) $(SOURCES_CPP): $(HEADERS_ASSETS)

# Include dependency files if they exist
# --------------------------------------

-include $(DEPS)
