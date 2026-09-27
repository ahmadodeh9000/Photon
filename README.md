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

Requires GTK3 and GtkSourceView 4 development headers.

```bash
# Debian / Ubuntu
sudo apt install libgtk-3-dev libgtksourceview-4-dev

# Arch
sudo pacman -S gtk3 gtksourceview4

# Fedora
sudo dnf install gtk3-devel gtksourceview4-devel

# macOS (Homebrew)
brew install gtk+3 gtksourceview4 pkg-config
```

```bash
git clone https://github.com/ahmadodeh9000/Photon
cd Photon
make
```

### macOS notes

Homebrew installs its libraries under `/opt/homebrew` on Apple Silicon or
`/usr/local` on Intel. If `make` can't find `pkg-config` or the GTK headers,
point it at Homebrew's prefix first:

```bash
export PKG_CONFIG_PATH="$(brew --prefix)/lib/pkgconfig:$PKG_CONFIG_PATH"
```

Run that once per terminal session (or add it to your `~/.zshrc`), then `make`
as usual.

The Lunar language spec and the color scheme need to be installed once:

```bash
make install-lang
```

Then run it:

```bash
./Photon
```

---

## Layout

```
editor.c                main source
lang/lunar.lang          Lunar syntax definition
styles/vscode-dark.xml   color scheme
icons/                   app icon, various sizes
Makefile
```

---

## License

MIT