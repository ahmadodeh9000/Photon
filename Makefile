# Photon: one Makefile for Linux and macOS.
#
#   make              build ./Photon (objects go to build/)
#   make deps         print the install command for this OS / distro
#   make check        headless smoke test (Linux CI: xvfb-run make check)
#   make test         unit tests: error parsers, editing (needs a display too)
#   make install      install to $(PREFIX) (default /usr/local); DESTDIR works
#   make uninstall
#   make app          macOS: build/Photon.app (icon, launcher, data inside)
#   make install-app  macOS: Photon.app into $(APPDIR) + a `photon` command in $(PREFIX)/bin
#   make uninstall-app
#   make font         rebuild fonts/PhotonMono-Regular.ttf from tools/font/glyphs.txt
#                     (needs: pip install -r tools/font/requirements.txt)
#   make WERROR=1     treat warnings as errors (CI does this)
#
# ./install.sh wraps all of this; see the README.

VERSION  := 0.1.0
TARGET   := Photon
BUILDDIR := build
PREFIX   ?= /usr/local
DATADIR  := $(PREFIX)/share/photon
APPDIR   ?= /Applications
APP      := $(BUILDDIR)/Photon.app
ICON_SIZES := 16 32 48 64 128 256 512
UNAME_S  := $(shell uname -s)

PKGS := gtk+-3.0 gtksourceview-4 vte-2.91 gio-unix-2.0   # gio-unix comes with GLib
ifeq ($(UNAME_S),Linux)
  PKGS += fontconfig      # bundled fonts (platform.c); GTK already depends on it
endif

# macOS: let pkg-config see Homebrew's .pc files (Apple Silicon or Intel).
# Set inline because make 3.81 (Apple's) doesn't export vars to $(shell).
PKG_CONFIG := pkg-config
ifeq ($(UNAME_S),Darwin)
  BREW_PREFIX := $(shell brew --prefix 2>/dev/null)
  ifneq ($(BREW_PREFIX),)
    PKG_CONFIG := PKG_CONFIG_PATH="$(BREW_PREFIX)/lib/pkgconfig:$(BREW_PREFIX)/share/pkgconfig:$$PKG_CONFIG_PATH" pkg-config
  endif
endif

# Only demand the libraries for targets that compile something
NO_DEPS_GOALS := deps clean uninstall uninstall-app font
ifneq ($(MAKECMDGOALS),)
ifeq ($(filter-out $(NO_DEPS_GOALS),$(MAKECMDGOALS)),)
  SKIP_PKG_CHECK := 1
endif
endif
ifndef SKIP_PKG_CHECK
  ifneq ($(shell $(PKG_CONFIG) --exists $(PKGS) && echo yes),yes)
    $(error GTK3 / GtkSourceView 4 / VTE dev packages not found. Run 'make deps' for the install command)
  endif
  PKG_CFLAGS := $(shell $(PKG_CONFIG) --cflags $(PKGS))
  PKG_LIBS   := $(shell $(PKG_CONFIG) --libs $(PKGS))
endif

WARNINGS := -Wall -Wextra
ifeq ($(WERROR),1)
  WARNINGS += -Werror
endif

CFLAGS   ?= -O2 -g
CFLAGS   += -std=gnu11 $(WARNINGS)
CPPFLAGS += $(PKG_CFLAGS) -MMD -MP \
            -DPHOTON_VERSION=\"$(VERSION)\" -DPHOTON_DATADIR=\"$(DATADIR)\"
LDLIBS   += $(PKG_LIBS)
ifeq ($(UNAME_S),Darwin)
  LDLIBS += -framework CoreText -framework CoreFoundation   # bundled fonts (platform.c)
endif

SRCS := $(wildcard src/*.c)
OBJS := $(patsubst src/%.c,$(BUILDDIR)/%.o,$(SRCS))
LIB_OBJS := $(filter-out $(BUILDDIR)/main.o,$(OBJS))   # everything but main(), for tests
TESTS := $(BUILDDIR)/test_errors $(BUILDDIR)/test_editing $(BUILDDIR)/test_app

.PHONY: all deps check test install uninstall app install-app uninstall-app font clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) $(OBJS) -o $@ $(LDLIBS)

$(BUILDDIR)/%.o: src/%.c | $(BUILDDIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILDDIR)/test_%: tests/test_%.c tests/check.h $(LIB_OBJS) | $(BUILDDIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(LIB_OBJS) -o $@ $(LDLIBS)

$(BUILDDIR):
	mkdir -p $@

-include $(OBJS:.o=.d)

# ---------------------------------------------------------------------
# make deps: print (not run) the exact install command
# ---------------------------------------------------------------------
deps:
ifeq ($(UNAME_S),Darwin)
	@echo "brew install pkg-config gtk+3 gtksourceview4 vte3"
else
	@. /etc/os-release 2>/dev/null; \
	case " $$ID $$ID_LIKE " in \
	  *" debian "*|*" ubuntu "*) echo "sudo apt install build-essential pkg-config libgtk-3-dev libgtksourceview-4-dev libvte-2.91-dev" ;; \
	  *" fedora "*|*" rhel "*)   echo "sudo dnf install gcc make pkgconf-pkg-config gtk3-devel gtksourceview4-devel vte291-devel" ;; \
	  *" arch "*)                echo "sudo pacman -S --needed base-devel gtk3 gtksourceview4 vte3" ;; \
	  *) echo "Unknown distro ($$ID). Install the development packages for: $(PKGS)" ;; \
	esac
endif

# ---------------------------------------------------------------------
# make check: start Photon on sample files, verify resources load, quit
# ---------------------------------------------------------------------
check: $(TARGET)
	./$(TARGET) --version
	./$(TARGET) --smoke-test tests/samples/hello.lunar

test: $(TESTS) $(TARGET)
	@for t in $(TESTS); do ./$$t || exit 1; done
	@python3 tests/test_launcher.py ./$(TARGET)

# ---------------------------------------------------------------------
# install / uninstall
# ---------------------------------------------------------------------
install: $(TARGET)
	install -d $(DESTDIR)$(PREFIX)/bin $(DESTDIR)$(DATADIR)/lang $(DESTDIR)$(DATADIR)/styles $(DESTDIR)$(DATADIR)/icons \
	           $(DESTDIR)$(DATADIR)/fonts
	install -m 755 $(TARGET) $(DESTDIR)$(PREFIX)/bin/photon
	install -m 644 fonts/*.ttf fonts/OFL.txt fonts/LICENSE $(DESTDIR)$(DATADIR)/fonts/
	install -m 644 lang/*.lang $(DESTDIR)$(DATADIR)/lang/
	install -m 644 styles/*.xml $(DESTDIR)$(DATADIR)/styles/
	install -m 644 icons/photon.png $(DESTDIR)$(DATADIR)/icons/photon.png
	for size in $(ICON_SIZES); do \
	    install -d $(DESTDIR)$(PREFIX)/share/icons/hicolor/$${size}x$${size}/apps; \
	    install -m 644 icons/photon-$$size.png $(DESTDIR)$(PREFIX)/share/icons/hicolor/$${size}x$${size}/apps/photon.png; \
	done
ifeq ($(UNAME_S),Linux)
	install -d $(DESTDIR)$(PREFIX)/share/applications
	sed 's|@BINDIR@|$(PREFIX)/bin|' data/photon.desktop > $(DESTDIR)$(PREFIX)/share/applications/photon.desktop
	chmod 644 $(DESTDIR)$(PREFIX)/share/applications/photon.desktop
endif

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/photon
	rm -rf $(DESTDIR)$(DATADIR)
	for size in $(ICON_SIZES); do rm -f $(DESTDIR)$(PREFIX)/share/icons/hicolor/$${size}x$${size}/apps/photon.png; done
	rm -f $(DESTDIR)$(PREFIX)/share/applications/photon.desktop

# ---------------------------------------------------------------------
# macOS app bundle:  Photon.app/Contents/
#   Info.plist        from data/Info.plist.in
#   MacOS/Photon      data/photon-launcher.sh (login PATH, starts in ~)
#   MacOS/photon-bin  the program
#   Resources/        photon.icns + lang/ styles/ icons/ (Photon finds them)
# ---------------------------------------------------------------------
APP_STAMP := $(APP)/Contents/Info.plist
APP_INPUTS := $(TARGET) data/Info.plist.in data/photon-launcher.sh icons/photon.icns icons/photon.png \
              $(wildcard lang/*.lang) $(wildcard styles/*.xml) $(wildcard fonts/*)

app: $(APP_STAMP)

# A real file target, so `sudo make install-app` only copies and never
# rebuilds (which would leave root-owned files in build/)
$(APP_STAMP): $(APP_INPUTS)
ifneq ($(UNAME_S),Darwin)
	$(error make app is for macOS; on Linux use make install)
endif
	rm -rf $(APP)
	install -d $(APP)/Contents/MacOS $(APP)/Contents/Resources/lang $(APP)/Contents/Resources/styles \
	           $(APP)/Contents/Resources/icons $(APP)/Contents/Resources/fonts
	install -m 755 data/photon-launcher.sh $(APP)/Contents/MacOS/Photon
	install -m 755 $(TARGET) $(APP)/Contents/MacOS/photon-bin
	install -m 644 icons/photon.icns $(APP)/Contents/Resources/photon.icns
	install -m 644 lang/*.lang $(APP)/Contents/Resources/lang/
	install -m 644 styles/*.xml $(APP)/Contents/Resources/styles/
	install -m 644 icons/photon.png $(APP)/Contents/Resources/icons/photon.png
	install -m 644 fonts/*.ttf fonts/OFL.txt fonts/LICENSE $(APP)/Contents/Resources/fonts/
	sed 's|@VERSION@|$(VERSION)|g' data/Info.plist.in > $@   # last: marks the bundle complete

# The `photon` command execs the program inside the bundle (a symlink wouldn't do:
# the program finds Photon.app from its own path). In launcher mode it starts the
# window through `open -a Photon.app`, so the Dock shows Photon with its icon.
install-app: $(APP_STAMP)
	install -d $(DESTDIR)$(APPDIR) $(DESTDIR)$(PREFIX)/bin
	rm -rf $(DESTDIR)$(APPDIR)/Photon.app
	cp -R $(APP) $(DESTDIR)$(APPDIR)/Photon.app
	printf '#!/bin/sh\nexec "%s/Photon.app/Contents/MacOS/photon-bin" "$$@"\n' "$(APPDIR)" > $(DESTDIR)$(PREFIX)/bin/photon
	chmod 755 $(DESTDIR)$(PREFIX)/bin/photon

uninstall-app:
	rm -rf $(DESTDIR)$(APPDIR)/Photon.app
	rm -f $(DESTDIR)$(PREFIX)/bin/photon

# ---------------------------------------------------------------------
# make font: Photon Mono from its pixel drawings (tools/font/)
# ---------------------------------------------------------------------
font:
	python3 tools/font/build.py --style smooth --out fonts/PhotonMono-Regular.ttf

clean:
	rm -rf $(BUILDDIR) $(TARGET)
