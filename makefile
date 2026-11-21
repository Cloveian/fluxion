# everything gets built from here
#
#   make               all of them: ce, pc, web and nspire, one after the other
#   make ce            bin-app/Fluxion.8ek, the CE app
#   make installer     bin-installer/, the app as appvars + its installer
#   make pc            build-pc/fluxion (PineappleCAS's command line) + build-pc/fluxion-app
#   make test          PC build + the engine's tests + the identity mask check
#   make budget        what each test line costs, the CE guess too
#   make web           web/build/, the app for a browser (needs emscripten)
#   make nspire        bin-nspire/Fluxion.tns, for Ndless on the CX and CX II (needs the Ndless SDK)
#   make clean         the CE build, make clean-pc for the PC / web one (build-pc/store goes too), clean-nspire
#   ./build-pc/fluxion simplify '2X+X'
#
# CE, PC and nspire goals don't mix in one call, the CE toolchain has its own test and clean

PC_PHONY = pc test budget web clean-pc
PC_GOALS = $(PC_PHONY) build-pc/% build-web/% web/%
NSPIRE_GOALS = nspire clean-nspire build-nspire/% bin-nspire/%

ifeq ($(MAKECMDGOALS),)

# plain make, each build in its own make so the goals don't mix
everything:
	$(MAKE) ce
	$(MAKE) pc
	$(MAKE) web
	$(MAKE) nspire

.PHONY: everything

else ifneq ($(filter $(PC_GOALS),$(MAKECMDGOALS)),)

# ----------------------------
# PC + web
# ----------------------------
# uses PineappleCAS's PC command line (src/engine/pc/). same flags as upstream:
# the engine is C89, and COMPILE_PC switches on the PC-only code.

CC      = gcc
CFLAGS  = -std=c89 -pedantic -g -DCOMPILE_PC -DDEBUG -Wall -Isrc/engine -Isrc -MMD -MP
LDLIBS  = -lm

SRCDIR  = src/engine
OBJDIR  = build-pc/obj
TARGET  = build-pc/fluxion

SOURCES := $(wildcard $(SRCDIR)/*.c) $(wildcard $(SRCDIR)/*/*.c)
OBJECTS := $(SOURCES:$(SRCDIR)/%.c=$(OBJDIR)/%.o)

pc: $(TARGET) build-pc/fluxion-app

$(TARGET): $(OBJECTS)
	@mkdir -p $(@D)
	$(CC) $(OBJECTS) $(LDLIBS) -o $@

$(OBJDIR)/%.o: $(SRCDIR)/%.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@

# Fluxion's own code (render, platform): C99, more warnings, SDL3 for the window
SDL_CFLAGS := $(shell pkg-config --cflags sdl3)
SDL_LIBS   := $(shell pkg-config --libs sdl3)
FLUX_CFLAGS = -std=c99 -g -DCOMPILE_PC -Wall -Wextra -Wno-missing-field-initializers \
              -Isrc/engine -Isrc -MMD -MP $(SDL_CFLAGS)

FLUX_SOURCES := $(wildcard src/render/*.c) $(wildcard src/platform/*.c) \
                $(wildcard src/editor/*.c) $(wildcard src/ui/*.c)
FLUX_OBJECTS := $(FLUX_SOURCES:src/%.c=build-pc/obj-flux/%.o)

build-pc/obj-flux/%.o: src/%.c
	@mkdir -p $(@D)
	$(CC) $(FLUX_CFLAGS) -c $< -o $@

# rebuild a .o when a header it includes changes
-include $(OBJECTS:.o=.d) $(FLUX_OBJECTS:.o=.d)

# everything but PineappleCAS's command line, for linking test programs
ENGINE_OBJECTS := $(filter-out $(OBJDIR)/pc/%,$(OBJECTS))

build-pc/identity-masks: test/identity_masks.c $(ENGINE_OBJECTS)
	$(CC) $(CFLAGS) $^ $(LDLIBS) -o $@

build-pc/layout-dump: test/layout_dump.c build-pc/obj-flux/render/layout.o $(ENGINE_OBJECTS)
	$(CC) $(FLUX_CFLAGS) $^ $(LDLIBS) -o $@

# draws expressions like the calculator: ./build-pc/view 'X^2/2'  (or --png out.png ...)
build-pc/view: test/view.c $(FLUX_OBJECTS) $(ENGINE_OBJECTS)
	$(CC) $(FLUX_CFLAGS) $^ $(LDLIBS) $(SDL_LIBS) -o $@

# the whole app in a window, keyboard as the keypad: ./build-pc/fluxion-app
build-pc/fluxion-app: src/main_pc.c $(FLUX_OBJECTS) $(ENGINE_OBJECTS)
	$(CC) $(FLUX_CFLAGS) $^ $(LDLIBS) $(SDL_LIBS) -o $@

# editor steps -> picture: ./build-pc/editor-test --png out.png t:2 frac t:1 D t:x
build-pc/editor-test: test/editor_test.c $(FLUX_OBJECTS) $(ENGINE_OBJECTS)
	$(CC) $(FLUX_CFLAGS) $^ $(LDLIBS) $(SDL_LIBS) -o $@

# the app in a browser, for the showcase page: make web -> web/build/
# same code, web.c stands in for the SDL bits. asyncify so keys_wait() can block
EMCC ?= emcc
WEB_FLAGS = -O2 -DCOMPILE_PC -Isrc/engine -Isrc -MMD -MP
WEB_SOURCES := $(filter-out $(SRCDIR)/pc/%,$(SOURCES)) src/main_pc.c \
               $(filter-out src/platform/keys_pc.c src/platform/window_pc.c,$(FLUX_SOURCES))
WEB_OBJECTS := $(WEB_SOURCES:src/%.c=build-web/%.o)

build-web/engine/%.o: src/engine/%.c
	@mkdir -p $(@D)
	$(EMCC) -std=c89 $(WEB_FLAGS) -c $< -o $@

build-web/%.o: src/%.c
	@mkdir -p $(@D)
	$(EMCC) -std=c99 $(WEB_FLAGS) -c $< -o $@

-include $(WEB_OBJECTS:.o=.d)

web/build/fluxion.js: $(WEB_OBJECTS)
	@mkdir -p $(@D)
	$(EMCC) $^ -O2 -lm -sASYNCIFY -sALLOW_MEMORY_GROWTH -sMODULARIZE -sEXPORT_ES6 -sENVIRONMENT=web \
		-sEXPORTED_RUNTIME_METHODS=HEAPU8,ccall -o $@

# the page reads the feature tables straight out of the docs
web/build/features.md: docs/features.md
	@mkdir -p $(@D)
	cp $< $@

web: web/build/fluxion.js web/build/features.md

test: $(TARGET) build-pc/identity-masks build-pc/layout-dump build-pc/view build-pc/editor-test build-pc/fluxion-app
	./$(TARGET) test test/pcas.tests
	./$(TARGET) test test/fluxion.tests
	./build-pc/identity-masks

budget: $(TARGET)
	./$(TARGET) budget test/fluxion.tests

clean-pc:
	rm -rf build-pc build-web web/build

.PHONY: $(PC_PHONY)

else ifneq ($(filter $(NSPIRE_GOALS),$(MAKECMDGOALS)),)

# ----------------------------
# nspire (Ndless, CX and CX II)
# ----------------------------
# the same code with keys_nspire.c and window_nspire.c in for the SDL bits, like
# the web build. screen_pc.c's framebuffer, store_pc.c's files and power_pc.c work as they are

NSPIRE_FLAGS = -Os -Wall -Isrc/engine -Isrc -MMD -MP
NSPIRE_SOURCES := $(filter-out src/engine/pc/%,$(wildcard src/engine/*.c src/engine/*/*.c)) src/main.c \
                  $(filter-out src/platform/keys_pc.c src/platform/window_pc.c src/platform/web.c, \
                    $(wildcard src/render/*.c src/platform/*.c src/editor/*.c src/ui/*.c))
NSPIRE_OBJECTS := $(NSPIRE_SOURCES:src/%.c=build-nspire/%.o)

build-nspire/engine/%.o: src/engine/%.c
	@mkdir -p $(@D)
	nspire-gcc -std=c89 $(NSPIRE_FLAGS) -c $< -o $@

build-nspire/%.o: src/%.c
	@mkdir -p $(@D)
	nspire-gcc -std=c99 $(NSPIRE_FLAGS) -c $< -o $@

-include $(NSPIRE_OBJECTS:.o=.d)

build-nspire/fluxion.elf: $(NSPIRE_OBJECTS)
	nspire-ld $^ -lm -o $@

# straight out of genzehn, current Ndless loads that. make-prg's loader is for old ones
bin-nspire/Fluxion.tns: build-nspire/fluxion.elf
	@mkdir -p $(@D)
	genzehn --input $< --output $@ --name Fluxion --compress --uses-lcd-blit true --240x320-support true

nspire: bin-nspire/Fluxion.tns

clean-nspire:
	rm -rf build-nspire bin-nspire

.PHONY: nspire clean-nspire

else

# ----------------------------
# CE (the toolchain's makefile does the actual building)
# ----------------------------

NAME = Fluxion
ICON = icon.png
DESCRIPTION = "Fluxion CAS"
COMPRESSED = YES

CFLAGS = -Wall -Wextra -Wno-missing-field-initializers -Oz
# CFLAGS += -g    # uncomment when debugging, makes the binary way bigger
CXXFLAGS = -Wall -Wextra -Oz

# the Flash App, own folders so an old program build in obj/ and bin/ stays out of it
APPLICATION = YES
OBJDIR = obj-app
BINDIR = bin-app

# program build (bin/FLUXION.8xp + appvars) is off, untested for a while. to bring it back drop the three lines above and uncomment, make installer only builds from the app tho
# NAME = FLUXION

# find the toolchain in ~/CEdev even when PATH doesn't have it (e.g. VS Code)
export PATH := $(HOME)/CEdev/bin:$(PATH)

include $(shell cedev-config --makefile)

# VS Code's Makefile Tools builds 'all' by default (kinda annoying, havent found how to stop it)
all: target
ce: target
.PHONY: all ce

# ----------------------------
# installer (see installer/README.md)
# ----------------------------
# 'make installer': Fluxion as a Flash App split into appvars, plus
# CEaShell-Installer (submodule, GPL-3.0) set up for Fluxion. builds from a copy
# in build-installer/ so the
# submodule never gets touched. stuff to send ends up in bin-installer/

INSTALLER_SRC = installer/ceashell-installer
INSTALLER_BUILD = build-installer
INSTALLER_OUT = bin-installer
INSTALLER_NAME = FLUXINST
INSTALLER_DESCRIPTION = "Fluxion Installer"
# appvar names are APPVAR_PREFIX + index (FLUXAPP0, FLUXAPP1, ...); the
# installer and convbin must agree on the prefix and the split size
APPVAR_PREFIX = FLUXAPP
APPVAR_SPLIT_SIZE = 65200

installer:
	$(MAKE)
	@test -f $(INSTALLER_SRC)/makefile || { echo "$(INSTALLER_SRC) is empty: run 'git submodule update --init'"; exit 1; }
	rm -rf $(INSTALLER_BUILD) $(INSTALLER_OUT)
	mkdir -p $(INSTALLER_BUILD) $(INSTALLER_OUT)
	git -C $(INSTALLER_SRC) archive --format=tar $$(git rev-parse :$(INSTALLER_SRC)) | tar -xf - -C $(INSTALLER_BUILD)
	$(MAKE) -C $(INSTALLER_BUILD) NAME=$(INSTALLER_NAME) DESCRIPTION='$(INSTALLER_DESCRIPTION)' ICON=$(CURDIR)/$(ICON) APPVAR_PREFIX=$(APPVAR_PREFIX) APPVAR_SPLIT_SIZE=$(APPVAR_SPLIT_SIZE)
	cp $(INSTALLER_BUILD)/bin/$(INSTALLER_NAME).8xp $(INSTALLER_OUT)/
	$(CONVBIN) --iformat 8ek --input $(BINDIR)/$(NAME).8ek --oformat 8xv-split --maxvarsize $(APPVAR_SPLIT_SIZE) --name $(APPVAR_PREFIX) --output $(INSTALLER_OUT)/$(APPVAR_PREFIX).8xv
	@ls -l $(INSTALLER_OUT)
.PHONY: installer

endif
