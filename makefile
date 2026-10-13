# ----------------------------
# makefile options
# ----------------------------

NAME = FLUXION
ICON = icon.png
DESCRIPTION = "Fluxion CAS"
COMPRESSED = YES

CFLAGS = -Wall -Wextra -Wno-missing-field-initializers -Oz
# CFLAGS += -g    # uncomment when debugging, makes the binary way bigger
CXXFLAGS = -Wall -Wextra -Oz

# ----------------------------

# find the toolchain in ~/CEdev even when PATH doesn't have it (e.g. VS Code)
export PATH := $(HOME)/CEdev/bin:$(PATH)

include $(shell cedev-config --makefile)

# VS Code's Makefile Tools builds 'all' by default (kinda annoying, havent found how to stop it)
all: target
.PHONY: all
