# Photon

A small 90s-styled code editor for [Lunar](https://github.com/ahmadodeh9000/Lunar),
C and Python, written in C on GTK3, GtkSourceView 4 and VTE. It runs on Linux
and macOS.

![Photon running a Lunar program in the Turbo 90s theme](docs/screenshot.png)

```
photon                  # open in the current directory
photon main.lunar       # open a file (it can be a new one)
photon a.c b.py         # several at once
photon ~/some/project   # open a folder in a new window, with the file tree shown
photon --run game.lunar # open a file and run it right away
photon -w notes.txt     # wait until that tab is closed
```

Like VS Code's `code`, `photon` returns to your prompt at once and prints
nothing: the window runs on its own and survives closing the terminal (its
output goes to `photon.log` in your cache folder, e.g. `~/.cache/photon/`).
If Photon is already open, the files open there as new tabs. `photon -w`
is for tools that wait for an editor, e.g.
`git config --global core.editor "photon -w"`. Use `--foreground` to keep
the window attached to the terminal while working on Photon itself, and
`photon --status` to list what the running Photon has open.

---

## Features

- **Lunar, C and Python**: highlighting, comment toggle, indent rules,
  auto-closing brackets and quotes, autocomplete with each language's
  keywords and built-ins (plus words from your open files)
- **Run** the current file in an interactive Run tab: `input()`, `scanf` and
  `os.input()` work, and so do SDL windows. It shows the exit code and time
- **Error markers**: errors from Lunar (compile and runtime, including imported
  files), gcc/clang and Python get a red line and a gutter mark, with the
  message in the status bar. Photon jumps to the first one, opening its file
- **Tabs** with an unsaved `*` mark; the files you had open come back next time
- **Find / replace**, go to line, duplicate and move lines, smart Enter in `{}`
- **Terminal** tab with your own shell
- **Themes** under View > Theme (or `photon --theme ID` for one run):

  | Theme | ID | Look |
  |---|---|---|
  | Turbo 90s (default) | `turbo90s` | Windows 95 chrome around a Borland Turbo C++ blue screen |
  | Notepad 95 | `notepad95` | the same chrome, black on white like the editors of the day |
  | Amber CRT | `amber` | an amber monochrome monitor, terminal included |
  | Green Phosphor | `green` | a green-screen terminal |
  | Hot Dog Stand | `hotdog` | red and mustard, after the loudest Windows 3.1 color scheme |
  | VS Code Dark | `vscode-dark` | the modern dark look |

- Zoom, auto save, a file tree that refreshes when you save

---

## Shortcuts

Cmd on macOS, Ctrl on Linux.

| Shortcut | Action |
|---|---|
| Cmd/Ctrl + N / O / S | New file (asks the language) / open / save |
| Cmd/Ctrl + Shift + S | Save as |
| Cmd/Ctrl + W | Close tab |
| Cmd/Ctrl + R or F5 | Run the current file |
| Cmd/Ctrl + . | Stop it |
| Cmd/Ctrl + F | Find |
| Cmd/Ctrl + Alt + F | Find and replace |
| Cmd/Ctrl + G / Shift + G | Next / previous match |
| Cmd/Ctrl + L | Go to line (`12` or `12:5`) |
| Cmd/Ctrl + / | Toggle comment (`//` or `#`) |
| Cmd/Ctrl + D | Duplicate line |
| Alt + Up / Down | Move line up / down |
| Cmd/Ctrl + B | Toggle the file tree |
| Cmd/Ctrl + \` or Cmd/Ctrl + T | Toggle the terminal |
| Cmd/Ctrl + + / - / 0 | Zoom in / out / back to your chosen size |
| Cmd/Ctrl + , | Fonts: code and interface font + size, with a live preview |
| Cmd/Ctrl + Q | Quit (asks about unsaved files) |

On Linux, while a terminal has focus it gets Ctrl keys first (Ctrl+C, Ctrl+D,
Ctrl+R... work as in any terminal); use Ctrl+Shift+C / V to copy and paste there.

---

## Install

```bash
git clone https://github.com/ahmadodeh9000/Photon
cd Photon
./install.sh --deps
```

That builds Photon and installs it **for you only** (no sudo):

- **macOS**: `~/Applications/Photon.app` (Launchpad, Dock and Spotlight show its
  icon) and a `photon` command in `~/.local/bin`
- **Linux**: `photon` in `~/.local/bin`, plus an entry with the icon in your
  applications menu

`--deps` installs the build dependencies first, after asking (Homebrew on macOS;
apt, dnf or pacman on Linux). Other options:

```bash
./install.sh --system        # for everyone: /Applications or /usr/local (uses sudo)
./install.sh --prefix DIR    # somewhere else
./install.sh --uninstall     # remove it again (add --system / --prefix if you used them)
```

If `~/.local/bin` isn't on your `PATH`, the installer tells you the line to add.

---

## Building by hand

You need GTK3, GtkSourceView 4 and VTE development packages. `make deps`
prints the exact install command for your system:

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
make            # finds Homebrew's pkg-config path on its own
./Photon
```

Nothing else needs installing: Photon loads `lang/` and `styles/` from next to
the binary. To install it system-wide (binary, language files, themes, icon,
and a `.desktop` entry on Linux):

```bash
sudo make install          # PREFIX=/usr/local by default; make uninstall undoes it
make app                   # macOS: build/Photon.app (make install-app copies it)
```

Other targets: `make test` (unit tests), `make check` (start, check, quit),
`make WERROR=1` (warnings are errors, as in CI).

### Running programs

- **Lunar**: Photon looks for `lunar` in `$LUNAR`, then in its settings, then in
  your `PATH`. If it can't find it, it asks once (Run > Locate Lunar... changes it).
- **Python**: `python3`, or `python` if there's no `python3`.
- **C**: `cc` (or `gcc`/`clang`) with `-Wall -g`; the binary goes to your temp
  folder. If there's a Makefile next to the file, Photon offers to run `make`.

### Fonts

The 90s themes use period fonts if you have them installed: a DOS font such as
[Px437 IBM VGA 8x16](https://int10h.org/oldschool-pc-fonts/), Fixedsys
Excelsior, and Pixelated MS Sans Serif for menus. Without them they use
Menlo / Monaco / Tahoma / Geneva on macOS and DejaVu on Linux, and still look
the part.

### The icon

The icon is 32x32 pixel art (a 90s monitor with the photon's γ on a Turbo-blue
screen), drawn as text in `tools/make_icon.py`. Edit the `ART` grid and run
`python3 tools/make_icon.py` to regenerate every size and the macOS `.icns`.

Settings live in `settings.ini` in your user config folder (`~/.config/photon`
on Linux).

---

## Adding a language

A language is **one entry** in `src/languages.c`:

```c
static const PhotonLanguage LANG_RUBY = {
    .id = "ruby", .name = "Ruby",
    .exts = { ".rb", NULL },
    .gsv_lang = "ruby",               // GtkSourceView highlighting id
    .run_cmd = "{ruby} {file}",       // run in the file's folder
    .comment = "#", .indent = 2,
    .pairs = "()[]{}\"\"''",          // auto-closing pairs
    .indent_after = "{[(",            // Enter after these indents
    .keywords = RUBY_WORDS,           // autocomplete words
    .error_parser = NULL,             // or one from src/errors.c
};
```

and add `&LANG_RUBY` to `LANGUAGES`. If GtkSourceView has no highlighting for
it, drop a `.lang` file in `lang/`; to run it with a new program, add one line
to `TOOLS` in `src/runner.c`. [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md)
covers the details, plus how to add menu items, shortcuts and themes.

---

## Layout

```
src/                     C sources, one module per job (see docs/ARCHITECTURE.md)
lang/lunar.lang          Lunar syntax definition
styles/                  code color schemes, one per theme
icons/                   app icon, every size + photon.icns (from tools/make_icon.py)
data/                    Linux .desktop entry, macOS Info.plist + launcher
install.sh               installer for macOS and Linux
tests/                   unit tests and sample Lunar / C / Python files
Makefile                 one Makefile for Linux and macOS
```

---

## License

MIT
