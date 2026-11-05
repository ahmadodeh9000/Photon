#!/bin/sh
# @author: Ahmad Odeh
#
# install.sh: build and install Photon on macOS or Linux.
#
#   ./install.sh               install for you (no sudo)
#                                Linux: ~/.local (bin, app menu entry, icon)
#                                macOS: ~/Applications/Photon.app + ~/.local/bin/photon
#   ./install.sh --system      install for everyone (asks for sudo if needed)
#                                Linux: /usr/local    macOS: /Applications + /usr/local/bin
#   ./install.sh --prefix DIR  install under DIR instead (Linux: DIR/bin, DIR/share;
#                                macOS: DIR/bin/photon + DIR/Applications/Photon.app)
#   ./install.sh --deps        also install the build dependencies (asks first)
#   ./install.sh --uninstall   remove what the same options installed
#   ./install.sh --yes         don't ask questions (for scripts)

set -eu

cd "$(dirname "$0")"

# ---------------------------------------------------------------------
# Options
# ---------------------------------------------------------------------
MODE=user
PREFIX=""
WANT_DEPS=0
UNINSTALL=0
ASSUME_YES=0

usage() {
    sed -n '4,15s/^# \{0,1\}//p' "$0"
    exit "${1:-0}"
}

while [ $# -gt 0 ]; do
    case "$1" in
        --system)    MODE=system ;;
        --prefix)    [ $# -ge 2 ] || usage 2; PREFIX="$2"; MODE=custom; shift ;;
        --prefix=*)  PREFIX="${1#--prefix=}"; MODE=custom ;;
        --deps)      WANT_DEPS=1 ;;
        --uninstall) UNINSTALL=1 ;;
        --yes|-y)    ASSUME_YES=1 ;;
        --help|-h)   usage 0 ;;
        *)           echo "Unknown option: $1" >&2; usage 2 ;;
    esac
    shift
done

OS="$(uname -s)"
case "$OS" in
    Darwin|Linux) ;;
    *) echo "Photon installs on macOS and Linux only (this is $OS)." >&2; exit 1 ;;
esac

# Where things go
case "$MODE" in
    user)   PREFIX="$HOME/.local"; APPDIR="$HOME/Applications" ;;
    system) PREFIX="/usr/local";   APPDIR="/Applications" ;;
    custom) APPDIR="$PREFIX/Applications" ;;
esac

# ---------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------
say()  { printf '\033[1m==> %s\033[0m\n' "$*"; }
fail() { printf 'Error: %s\n' "$*" >&2; exit 1; }

ask() {   # ask "question" -> 0 for yes
    [ "$ASSUME_YES" -eq 1 ] && return 0
    printf '%s [y/N] ' "$1"
    read -r answer || return 1
    case "$answer" in y|Y|yes|YES) return 0 ;; *) return 1 ;; esac
}

# Run as root only when the target folder isn't writable
SUDO=""
needs_sudo() {
    dir="$1"
    while [ ! -d "$dir" ]; do dir="$(dirname "$dir")"; done
    [ -w "$dir" ] || SUDO="sudo"
}

# Prints the package manager command for this system, or nothing
deps_command() {
    if [ "$OS" = Darwin ]; then
        command -v brew >/dev/null 2>&1 && echo "brew install pkg-config gtk+3 gtksourceview4 vte3"
        return 0
    fi
    # shellcheck disable=SC1091  # the distro's own file
    [ -r /etc/os-release ] && . /etc/os-release
    case " ${ID:-} ${ID_LIKE:-} " in
        *" debian "*|*" ubuntu "*) echo "sudo apt-get install -y build-essential pkg-config libgtk-3-dev libgtksourceview-4-dev libvte-2.91-dev" ;;
        *" fedora "*|*" rhel "*)   echo "sudo dnf install -y gcc make pkgconf-pkg-config gtk3-devel gtksourceview4-devel vte291-devel" ;;
        *" arch "*)                echo "sudo pacman -S --needed --noconfirm base-devel gtk3 gtksourceview4 vte3" ;;
    esac
}

have_deps() {
    pc_path="${PKG_CONFIG_PATH:-}"
    if [ "$OS" = Darwin ] && command -v brew >/dev/null 2>&1; then
        brew_prefix="$(brew --prefix)"
        pc_path="$brew_prefix/lib/pkgconfig:$brew_prefix/share/pkgconfig:$pc_path"
    fi
    command -v make >/dev/null 2>&1 && command -v cc >/dev/null 2>&1 &&
        PKG_CONFIG_PATH="$pc_path" pkg-config --exists gtk+-3.0 gtksourceview-4 vte-2.91 2>/dev/null
}

# ---------------------------------------------------------------------
# Uninstall
# ---------------------------------------------------------------------
if [ "$UNINSTALL" -eq 1 ]; then
    if [ "$OS" = Darwin ]; then
        needs_sudo "$APPDIR"; needs_sudo "$PREFIX"
        say "Removing $APPDIR/Photon.app and $PREFIX/bin/photon"
        $SUDO make uninstall-app PREFIX="$PREFIX" APPDIR="$APPDIR"
    else
        needs_sudo "$PREFIX"
        say "Removing Photon from $PREFIX"
        $SUDO make uninstall PREFIX="$PREFIX"
    fi
    say "Done. (Your settings stay in ~/.config/photon.)"
    exit 0
fi

# ---------------------------------------------------------------------
# 1. Dependencies
# ---------------------------------------------------------------------
if ! have_deps; then
    cmd="$(deps_command)"
    if [ "$OS" = Darwin ] && ! command -v brew >/dev/null 2>&1; then
        fail "Photon needs Homebrew on macOS: https://brew.sh, then run this again with --deps."
    fi
    [ -n "$cmd" ] || fail "missing GTK3 / GtkSourceView 4 / VTE development packages, and I don't know this distro's package manager. Install them, then run this again."

    if [ "$WANT_DEPS" -eq 1 ] && ask "Install the build dependencies with: $cmd ?"; then
        say "Installing dependencies"
        sh -c "$cmd"
    else
        echo "Photon needs some development packages first. Install them with:"
        echo
        echo "    $cmd"
        echo
        echo "or run: ./install.sh --deps"
        exit 1
    fi
    have_deps || fail "the dependencies still aren't found by pkg-config."
fi

# ---------------------------------------------------------------------
# 2. Build
# ---------------------------------------------------------------------
say "Building Photon"
jobs="$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 2)"
make -j"$jobs" PREFIX="$PREFIX"
./Photon --version >/dev/null || fail "the build doesn't start."

# ---------------------------------------------------------------------
# 3. Install
# ---------------------------------------------------------------------
if [ "$OS" = Darwin ]; then
    needs_sudo "$APPDIR"; needs_sudo "$PREFIX"
    say "Installing $APPDIR/Photon.app and $PREFIX/bin/photon"
    make app PREFIX="$PREFIX"
    $SUDO make install-app PREFIX="$PREFIX" APPDIR="$APPDIR"
    # Make Finder / Launchpad pick up the icon right away
    touch "$APPDIR/Photon.app" 2>/dev/null || true
else
    needs_sudo "$PREFIX"
    say "Installing to $PREFIX"
    $SUDO make install PREFIX="$PREFIX"
    # Refresh the app menu and icon caches when the tools are there
    if command -v update-desktop-database >/dev/null 2>&1; then
        $SUDO update-desktop-database -q "$PREFIX/share/applications" 2>/dev/null || true
    fi
    if command -v gtk-update-icon-cache >/dev/null 2>&1 && [ -f "$PREFIX/share/icons/hicolor/index.theme" ]; then
        $SUDO gtk-update-icon-cache -q -t "$PREFIX/share/icons/hicolor" 2>/dev/null || true
    fi
fi

# ---------------------------------------------------------------------
# 4. Check it and say how to start it
# ---------------------------------------------------------------------
"$PREFIX/bin/photon" --version >/dev/null || fail "installed, but $PREFIX/bin/photon doesn't run."

say "Photon is installed"
if [ "$OS" = Darwin ]; then
    echo "  Open it from Launchpad / $APPDIR, or type: photon"
else
    echo "  Open it from your applications menu, or type: photon"
fi

case ":$PATH:" in
    *":$PREFIX/bin:"*) ;;
    *)
        echo
        echo "  $PREFIX/bin isn't on your PATH yet. Add this to your shell's startup file"
        echo "  (~/.zshrc on macOS, ~/.bashrc on most Linux systems):"
        echo
        echo "    export PATH=\"$PREFIX/bin:\$PATH\""
        ;;
esac
