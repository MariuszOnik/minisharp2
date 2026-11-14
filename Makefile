# MiniSharp2 – budowanie na PC (tylko testy).
#   make test    buduje build/minisharp i testy jednostkowe (tests/unit/*.c), uruchamia tests/run_tests.sh
#   make         samo budowanie (bez hosta platform/pc: tylko kompilacja warstw do build/obj)
#   make clean   usuwa build/
# Na Windows uruchamiaj przez tools/test.ps1 (ustawia PATH do msys64).
#   make 3ds     build/MiniSharp.3dsx (devkitPro, platform/3ds/Makefile); workflow woła go, gdy istnieje platform/3ds.

ifeq ($(origin CC),default)
CC := gcc
endif
CFLAGS ?= -O2 -g -Wall -Wextra -std=c99 -D_DEFAULT_SOURCE
BUILD  := build

# Katalog tymczasowy w build/: gcc z msys64 uruchomiony spoza MSYS nie widzi domyślnego TMP i pada na C:\windows\.
TMPDIR := $(CURDIR)/$(BUILD)/tmp
export TMP := $(TMPDIR)
export TEMP := $(TMPDIR)
export TMPDIR

ifeq ($(OS),Windows_NT)
EXE := .exe
endif
BIN := $(BUILD)/minisharp$(EXE)

# Warstwy, które wchodzą do binarki testowej (kolejne dochodzą w etapach 1–3).
# moduły: modules/ (lista) i każdy modules/<nazwa>/ – nowy folder wchodzi do buildu sam
MODDIRS := modules $(patsubst %/,%,$(wildcard modules/*/))
SRCDIRS := lang lib graph app $(MODDIRS) platform/pc
SRC     := $(strip $(foreach d,$(SRCDIRS),$(wildcard $(d)/*.c)))
HDR     := $(strip $(foreach d,$(SRCDIRS),$(wildcard $(d)/*.h)))
INC     := $(foreach d,$(SRCDIRS),-I$(d))

OBJ     := $(SRC:%.c=$(BUILD)/obj/%.o)
# Binarkę testową linkujemy, gdy jest host PC z main() (etap 3); wcześniej tylko kompilujemy warstwy.
HOST    := $(wildcard platform/pc/*.c)

# Testy jednostkowe w C: każdy tests/unit/X.c to osobny program linkowany z warstwami bez hosta (bez main z platform/pc).
CORE_OBJ := $(filter-out $(BUILD)/obj/platform/%,$(OBJ))
UNIT_SRC := $(wildcard tests/unit/*.c)
UNIT_BIN := $(UNIT_SRC:tests/unit/%.c=$(BUILD)/unit/%$(EXE))

.PHONY: all test clean 3ds

# Nintendo 3DS (devkitPro): build/MiniSharp.3dsx; Makefile of the platform in platform/3ds.
3ds:
	@$(MAKE) --no-print-directory -C platform/3ds TOPDIR=$(CURDIR)

all: $(if $(HOST),$(BIN),$(if $(SRC),$(OBJ),nothing))

$(BIN): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $(OBJ) -lm

$(BUILD)/obj/%.o: %.c $(HDR) | $(TMPDIR)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(INC) -c -o $@ $<

$(BUILD)/unit/%$(EXE): tests/unit/%.c $(CORE_OBJ) $(HDR) | $(TMPDIR)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(INC) -o $@ $< $(CORE_OBJ) -lm

$(TMPDIR):
	@mkdir -p $@

nothing:
	@echo "Brak plikow .c w $(SRCDIRS) - nie ma czego budowac."

test: all $(UNIT_BIN)
	@BIN="$(BIN)" UNIT="$(UNIT_BIN)" sh tests/run_tests.sh

clean:
	rm -rf $(BUILD)
