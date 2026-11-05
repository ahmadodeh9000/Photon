# Photon

A small code editor built on GTK3 and GtkSourceView, written in C.

It exists mainly to edit [Lunar](https://github.com/ahmadodeh9000/Lunar) source
files with real syntax highlighting, but it works fine as a general C editor too.

```
photon                # open in the current directory
photon main.lunar      # open a file directly
photon ~/some/project  # open a directory
```

---

## Features

- Syntax highlighting for C and Lunar, switched automatically by file extension
- File tree sidebar rooted at the working directory
- Open / Save through native GTK file dialogs
- A VS Code Dark+ style scheme
- A monospace font stack with sane fallbacks

---

## Building

Requires GTK3, GtkSourceView 4 and VTE development packages. `make deps`
prints the exact install command for your OS / distro:

```bash
# Debian / Ubuntu
sudo apt install build-essential pkg-config libgtk-3-dev libgtksourceview-4-dev libvte-2.91-dev

# Fedora
sudo dnf install gcc make pkgconf-pkg-config gtk3-devel gtksourceview4-devel vte291-devel

# Arch
sudo pacman -S --needed base-devel gtk3 gtksourceview4 vte3

# macOS (Homebrew, Apple Silicon or Intel)
brew install pkg-config gtk+3 gtksourceview4 vte3
```

```bash
git clone https://github.com/ahmadodeh9000/Photon
cd Photon
make            # the Makefile finds Homebrew's pkg-config path on its own
./Photon
```

Nothing else needs installing: Photon loads `lang/` and `styles/` from next to
the binary. To install system-wide (binary, language files, schemes, icon, and
a `.desktop` entry on Linux):

```bash
sudo make install          # PREFIX=/usr/local by default
```

---

## Layout

```
src/                     C sources, one module per job (see docs/ARCHITECTURE.md)
lang/lunar.lang          Lunar syntax definition
styles/vscode-dark.xml   color scheme
icons/                   app icon
data/photon.desktop      Linux desktop entry
tests/samples/           sample Lunar / C / Python files
Makefile                 one Makefile for Linux and macOS
```

---

## License

MIT