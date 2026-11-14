#---------------------------------------------------------------------------------
# MiniSharp2 for Nintendo 3DS (devkitPro / devkitARM). Called from the top Makefile: `make 3ds` -> build/MiniSharp.3dsx
# Needs DEVKITPRO and DEVKITARM (Windows: see CLAUDE.md; CI: the devkitpro/devkitarm container).
#---------------------------------------------------------------------------------
.SUFFIXES:

ifeq ($(strip $(DEVKITARM)),)
$(error "Please set DEVKITARM in your environment. export DEVKITARM=<path to>devkitARM")
endif

TOPDIR ?= $(abspath $(CURDIR)/../..)
include $(DEVKITARM)/3ds_rules

TARGET   := MiniSharp
# the core layers + this platform (file names must be unique across the folders: the objects go into one directory)
SOURCES  := lang lib graph app platform/3ds
BUILD    := build/obj3ds

# build identifier (commit, "+" = uncommitted changes, date), shown in the Homebrew Launcher and in the menu
GIT_ID     ?= $(shell git -C $(TOPDIR) rev-parse --short HEAD 2>/dev/null)$(shell git -C $(TOPDIR) diff --quiet HEAD -- 2>/dev/null || echo +)
BUILD_TIME ?= $(shell date +"%Y-%m-%d %H:%M")
export GIT_ID BUILD_TIME
# BUILD_NUM: the number of the GitHub Actions run (make 3ds BUILD_NUM=57), empty for a local build
BUILD_ID        := $(if $(BUILD_NUM),build $(BUILD_NUM) / )$(GIT_ID), $(BUILD_TIME)
APP_TITLE       := MiniSharp2 $(if $(BUILD_NUM),nr $(BUILD_NUM) )$(BUILD_TIME)
APP_DESCRIPTION := $(BUILD_ID)
APP_AUTHOR      := MariuszOnik

ARCH     := -march=armv6k -mtune=mpcore -mfloat-abi=hard -mtp=soft
CFLAGS   := -g -Wall -O2 -mword-relocations -ffunction-sections -std=gnu11 $(ARCH)
CFLAGS   += $(INCLUDE) -D__3DS__ -DBUILD_ID='"$(BUILD_ID)"'
ASFLAGS  := -g $(ARCH)
LDFLAGS   = -specs=3dsx.specs -g $(ARCH) -Wl,-Map,$(notdir $*.map)
LIBS     := -lcitro2d -lcitro3d -lctru -lm
LIBDIRS  := $(CTRULIB)

ifneq ($(notdir $(BUILD)),$(notdir $(CURDIR)))

export OUTPUT   := $(TOPDIR)/build/$(TARGET)
export VPATH    := $(foreach dir,$(SOURCES),$(TOPDIR)/$(dir))
export DEPSDIR  := $(TOPDIR)/$(BUILD)

CFILES          := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(TOPDIR)/$(dir)/*.c)))
export LD       := $(CC)
export OFILES   := $(CFILES:.c=.o)
export INCLUDE  := $(foreach dir,$(SOURCES),-I$(TOPDIR)/$(dir)) \
                   $(foreach dir,$(LIBDIRS),-I$(dir)/include)
export LIBPATHS := $(foreach dir,$(LIBDIRS),-L$(dir)/lib)

export APP_ICON   := $(TOPDIR)/platform/3ds/icon.png
export _3DSXDEPS  := $(OUTPUT).smdh
export _3DSXFLAGS += --smdh=$(OUTPUT).smdh
# sdk/ (the documentation chapters for Help) goes into the .3dsx as romfs:/
export _3DSXFLAGS += --romfs=$(TOPDIR)/sdk

.PHONY: all clean

all:
	@mkdir -p $(TOPDIR)/$(BUILD)
	@rm -f $(OUTPUT).smdh $(TOPDIR)/$(BUILD)/main_3ds.o
	@$(MAKE) --no-print-directory -C $(TOPDIR)/$(BUILD) -f $(TOPDIR)/platform/3ds/Makefile TOPDIR=$(TOPDIR)

clean:
	@rm -fr $(TOPDIR)/$(BUILD) $(OUTPUT).3dsx $(OUTPUT).smdh $(OUTPUT).elf

else

$(OUTPUT).3dsx : $(OUTPUT).elf $(_3DSXDEPS)
$(OUTPUT).elf  : $(OFILES)

-include $(DEPSDIR)/*.d

endif
