# PC build of the engine, for fast tests without the emulator.
#
#   make -f pc.makefile          build build-pc/fluxion
#   make -f pc.makefile test     run the engine's tests + the identity mask check
#   ./build-pc/fluxion simplify '2X+X'
#
# uses PineappleCAS's PC command line (src/engine/pc/). same flags as upstream:
# the engine is C89, and COMPILE_PC switches on the PC-only code.

CC      = gcc
CFLAGS  = -std=c89 -pedantic -g -DCOMPILE_PC -DDEBUG -Wall -Isrc/engine -MMD -MP
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

# rebuild a .o when a header it includes changes
-include $(OBJECTS:.o=.d)

# everything but PineappleCAS's command line, for linking test programs
ENGINE_OBJECTS := $(filter-out $(OBJDIR)/pc/%,$(OBJECTS))

build-pc/identity-masks: test/identity_masks.c $(ENGINE_OBJECTS)
	$(CC) $(CFLAGS) $^ $(LDLIBS) -o $@

test: $(TARGET) build-pc/identity-masks
	./$(TARGET) test test/pcas-tests.txt
	./build-pc/identity-masks

clean:
	rm -rf build-pc

.PHONY: test clean
