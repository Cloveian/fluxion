# ----------------------------
# makefile options
# ----------------------------

NAME = FLUXION
ICON = icon.png
DESCRIPTION = "Fluxion CAS"
COMPRESSED = NO

CFLAGS = -Wall -Wextra -Oz
CXXFLAGS = -Wall -Wextra -Oz

# ----------------------------

# find the toolchain in ~/CEdev even when PATH doesn't have it (e.g. VS Code)
export PATH := $(HOME)/CEdev/bin:$(PATH)

include $(shell cedev-config --makefile)

# VS Code's Makefile Tools builds 'all' by default (kinda annoying, havent found how to stop it)
all: target
.PHONY: all
