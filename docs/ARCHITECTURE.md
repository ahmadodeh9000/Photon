# Photon architecture

Photon is plain C on GTK3 + GtkSourceView 4 + VTE. Each file in `src/` owns
one job and says so in the comment at its top. There are no globals: the
window's state lives in a `PhotonApp` (see `app.h`), and every callback gets
it as `user_data`.

```
src/main.c        startup: arguments, --version / --smoke-test / --screenshot,
                  finds the data folder, then calls photon_app_new()
src/app.c/.h      main window: layout, menus + shortcuts, title, file dialogs
src/document.c/.h one open file: GtkSourceBuffer + view + path + dirty state
src/sidebar.c/.h  file tree on the left (lazy: folders are read on expand)
src/panel.c/.h    bottom panel: the VTE terminal running the user's shell
src/languages.c/.h file -> GtkSourceView language; adds lang/ to the search path
src/theme.c/.h    GTK CSS for the chrome + the code style scheme
src/platform.c/.h ALL OS-specific code (#ifdef __APPLE__ / __linux__)

lang/             GtkSourceView language specs (lunar.lang)
styles/           GtkSourceView style schemes (vscode-dark.xml)
icons/            app icon (1024 px + a 256 px copy for Linux icon themes)
data/             photon.desktop (Linux only)
tests/samples/    small Lunar / C / Python files for manual and CI checks
```

How it fits together:

```
main ──> platform_data_dir() ──> languages_init() + theme_init()
     └─> app_new()
           ├─ document_new()   editor (center)
           ├─ panel_new()      terminal (bottom, hidden at start)
           └─ sidebar_new()    file tree (left), calls back app_open_file()
```

Each module follows the same shape: an opaque struct (`PhotonSidebar`, ...),
`photon_<module>_new()` / `_free()`, `_get_widget()` for the root widget the
app packs into the layout, and a few verbs (`photon_panel_toggle()`, ...).
A module never includes `app.h` unless it really needs the whole app.

### Data files at runtime

Nothing has to be installed for highlighting. At startup
`photon_platform_data_dir()` finds the folder holding `lang/` and `styles/`
by checking, in order: `$PHOTON_DATA_DIR`, the folder of the executable
(running `./Photon` from the repo), its parent, `<prefix>/share/photon`
next to an installed binary, and finally the compiled-in `PHOTON_DATADIR`.
`lang/` is put **first** in GtkSourceView's search path, so the repo's
`lunar.lang` beats an older copy in `~/.local/share/gtksourceview-4`.

### Rules

- Shortcuts are always written with `<Primary>` (Cmd on macOS, Ctrl on Linux).
  Avoid Cmd+H / Cmd+M (macOS) and Ctrl+Alt+arrows (Linux).
- `#ifdef __APPLE__` / `__linux__` only in `platform.c`.
- Paths are built with `g_build_filename()`; temp/config dirs come from
  `g_get_tmp_dir()` / `g_get_user_config_dir()`; programs are found with
  `g_find_program_in_path()`. Never hardcode `/tmp`, `~/.config` or
  `/opt/homebrew`.
- Every font list ends in something both OSes have (Menlo, DejaVu Sans Mono,
  Liberation Mono, `monospace`).
- `make WERROR=1` must build with zero warnings (CI does this on Linux and macOS).
  Unused callback parameters are marked `G_GNUC_UNUSED`.

---

## How to add a feature

1. Decide which module owns it. New area of the UI? New module:
   `src/thing.c` + `src/thing.h` with the header comment, an opaque
   `PhotonThing`, and `photon_thing_new/free/get_widget`.
2. If the window needs to hold it, add a field to `PhotonApp` in `app.h`,
   create it in `photon_app_new()` and free it in `photon_app_free()`.
3. The Makefile compiles every `src/*.c`, so there is nothing to register.
4. `make WERROR=1 && make check`.

## How to add a menu item + shortcut

In `app.c`, write a callback:

```c
static void
on_reload(GtkWidget *item G_GNUC_UNUSED, gpointer user_data) {
    PhotonApp *app = user_data;
    ...
}
```

and add one line to the matching `build_*_menu()` function:

```c
add_item(menu, accel, "Reload", "<Primary><Shift>r", G_CALLBACK(on_reload), app);
```

Pass `NULL` as the shortcut for a menu item without one.

## How to add a language

Today a language is picked by GtkSourceView from the file extension (the
`globs` in its `.lang` file), so:

- **Built-in to GtkSourceView** (C, Python, ...): nothing to do.
- **Your own**: drop `lang/<id>.lang` in the repo (copy `lunar.lang` as a
  starting point; style ids are `<id>:<name>` and should `map-to` a `def:*`
  style so every scheme colors them), and optionally add `<id>:*` colors to
  the schemes in `styles/`. `make install` copies every `lang/*.lang`.

(Phase 2 replaces this with a one-entry registry in `languages.c` that also
holds the run command, comment marker, indent and error parser.)
