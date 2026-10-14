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

# Flash App build (make APPLICATION=YES, used by 'make installer'): the app
# gets its own name and folders so it doesn't clobber the program build in
# obj/ and bin/
APP_NAME = Fluxion
ifeq ($(APPLICATION),YES)
NAME = $(APP_NAME)
OBJDIR = obj-app
BINDIR = bin-app
endif

# ----------------------------

# find the toolchain in ~/CEdev even when PATH doesn't have it (e.g. VS Code)
export PATH := $(HOME)/CEdev/bin:$(PATH)

include $(shell cedev-config --makefile)

# VS Code's Makefile Tools builds 'all' by default (kinda annoying, havent found how to stop it)
all: target
.PHONY: all

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
	$(MAKE) APPLICATION=YES
	@test -f $(INSTALLER_SRC)/makefile || { echo "$(INSTALLER_SRC) is empty: run 'git submodule update --init'"; exit 1; }
	rm -rf $(INSTALLER_BUILD) $(INSTALLER_OUT)
	mkdir -p $(INSTALLER_BUILD) $(INSTALLER_OUT)
	git -C $(INSTALLER_SRC) archive --format=tar $$(git rev-parse :$(INSTALLER_SRC)) | tar -xf - -C $(INSTALLER_BUILD)
	$(MAKE) -C $(INSTALLER_BUILD) NAME=$(INSTALLER_NAME) DESCRIPTION='$(INSTALLER_DESCRIPTION)' ICON=$(CURDIR)/$(ICON) APPVAR_PREFIX=$(APPVAR_PREFIX) APPVAR_SPLIT_SIZE=$(APPVAR_SPLIT_SIZE)
	cp $(INSTALLER_BUILD)/bin/$(INSTALLER_NAME).8xp $(INSTALLER_OUT)/
	$(CONVBIN) --iformat 8ek --input bin-app/$(APP_NAME).8ek --oformat 8xv-split --maxvarsize $(APPVAR_SPLIT_SIZE) --name $(APPVAR_PREFIX) --output $(INSTALLER_OUT)/$(APPVAR_PREFIX).8xv
	@ls -l $(INSTALLER_OUT)
.PHONY: installer
