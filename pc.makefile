# PC build of the engine, for fast tests without the emulator.
#
#   make -f pc.makefile          build build-pc/fluxion
#   make -f pc.makefile test     run the engine's tests + the identity mask check
#   ./build-pc/fluxion simplify '2X+X'
#
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

test: $(TARGET) build-pc/identity-masks build-pc/layout-dump build-pc/view build-pc/editor-test build-pc/fluxion-app
	./$(TARGET) test test/pcas-tests.txt
	./$(TARGET) test test/fluxion-tests.txt
	./build-pc/identity-masks

clean:
	rm -rf build-pc

.PHONY: test clean
