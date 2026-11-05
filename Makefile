# Photon: one Makefile for Linux and macOS.
#
#   make              build ./Photon (objects go to build/)
#   make deps         print the install command for this OS / distro
#   make check        headless smoke test (Linux CI: xvfb-run make check)
#   make install      install to $(PREFIX) (default /usr/local); DESTDIR works
#   make uninstall
#   make WERROR=1     treat warnings as errors (CI does this)

VERSION  := 0.1.0
TARGET   := Photon
BUILDDIR := build
PREFIX   ?= /usr/local
DATADIR  := $(PREFIX)/share/photon
UNAME_S  := $(shell uname -s)

PKGS := gtk+-3.0 gtksourceview-4 vte-2.91

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
NO_DEPS_GOALS := deps clean uninstall
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

SRCS := $(wildcard src/*.c)
OBJS := $(patsubst src/%.c,$(BUILDDIR)/%.o,$(SRCS))

.PHONY: all deps check install uninstall clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) $(OBJS) -o $@ $(LDLIBS)

$(BUILDDIR)/%.o: src/%.c | $(BUILDDIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

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

# ---------------------------------------------------------------------
# install / uninstall
# ---------------------------------------------------------------------
install: $(TARGET)
	install -d $(DESTDIR)$(PREFIX)/bin $(DESTDIR)$(DATADIR)/lang $(DESTDIR)$(DATADIR)/styles \
	           $(DESTDIR)$(DATADIR)/icons $(DESTDIR)$(PREFIX)/share/icons/hicolor/256x256/apps
	install -m 755 $(TARGET) $(DESTDIR)$(PREFIX)/bin/photon
	install -m 644 lang/*.lang $(DESTDIR)$(DATADIR)/lang/
	install -m 644 styles/*.xml $(DESTDIR)$(DATADIR)/styles/
	install -m 644 icons/photon.png $(DESTDIR)$(DATADIR)/icons/photon.png
	install -m 644 icons/photon-256.png $(DESTDIR)$(PREFIX)/share/icons/hicolor/256x256/apps/photon.png
ifeq ($(UNAME_S),Linux)
	install -d $(DESTDIR)$(PREFIX)/share/applications
	install -m 644 data/photon.desktop $(DESTDIR)$(PREFIX)/share/applications/photon.desktop
endif

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/photon
	rm -rf $(DESTDIR)$(DATADIR)
	rm -f $(DESTDIR)$(PREFIX)/share/icons/hicolor/256x256/apps/photon.png
	rm -f $(DESTDIR)$(PREFIX)/share/applications/photon.desktop

clean:
	rm -rf $(BUILDDIR) $(TARGET)
