# Photon architecture

Photon is plain C on GTK3 + GtkSourceView 4 + VTE. Each file in `src/` owns
one job and says so in the comment at its top. There are no globals: the
window's state lives in a `PhotonApp` (see `app.h`), and every callback gets
it as `user_data`.

```
src/main.c         startup: arguments (--version, --run, --smoke-test,
                   --screenshot), finds the data folder, calls photon_app_new()
src/app.c/.h       main window: layout, menus + shortcuts, title, file dialogs,
                   save, showing run errors
src/document.c/.h  one open file: buffer + view + path + language + dirty
                   state + error markers (underline, gutter dot, tooltip)
src/editing.c/.h   typing behaviors from the language: auto-close pairs,
                   comment toggle
src/sidebar.c/.h   file tree on the left (lazy: folders are read on expand)
src/panel.c/.h     bottom notebook; owns the "Terminal" tab (user's shell)
src/runner.c/.h    Run / Stop: builds the command, runs it in the "Run" tab,
                   tees stderr, exit code + time, finds lunar/python3/cc
src/errors.c/.h    stderr parsers (Lunar, gcc/clang, Python) -> PhotonError list
src/languages.c/.h the language registry (one entry per language)
src/statusbar.c/.h message (errors in red) + run status at the bottom
src/settings.c/.h  settings.ini (GKeyFile) in g_get_user_config_dir()/photon
src/theme.c/.h     GTK CSS for the chrome + the code style scheme
src/platform.c/.h  ALL OS-specific code (#ifdef __APPLE__ / __linux__)

lang/             GtkSourceView language specs (lunar.lang)
styles/           GtkSourceView style schemes (vscode-dark.xml)
icons/            app icon (1024 px + a 256 px copy for Linux icon themes)
data/             photon.desktop (Linux only)
tests/samples/    small Lunar / C / Python files for manual and CI checks
tests/test_*.c    unit tests (`make test`): parsers, editing, markers, registry
```

How it fits together:

```
main ──> platform_init() + platform_data_dir() ──> languages_init() + theme_init()
     └─> app_new()
           ├─ settings_load()
           ├─ statusbar_new()  bottom strip
           ├─ document_new()   editor (center), attaches editing.c
           ├─ panel_new()      bottom notebook: Terminal tab (hidden at start)
           ├─ runner_new()     adds the Run tab to the panel
           └─ sidebar_new()    file tree (left), calls back app_open_file()
```

A run, start to finish:

```
Run (Cmd/Ctrl+R, F5) -> runner_run()
  app_save()                         asks for a name if untitled
  language = document_get_language() registry entry for the file
  command  = run_cmd with {file} {exe} {lunar} ... expanded and quoted
             ({lunar}: $LUNAR, settings.ini [tools] lunar=, PATH, then ask)
  /bin/sh RUN_WRAPPER err-file command   in the Run tab's VteTerminal
        stdin/stdout on the terminal, stderr tee'd to err-file
child-exited -> "[Exit N · 0.42 s]", status bar
  errors = language->error_parser(err-file text)
  app_show_errors(): open the file of the first error, mark lines, jump
Any edit in the document clears its markers.
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

Add ONE entry to `LANGUAGES` in `src/languages.c`:

```c
static const PhotonLanguage LANG_RUBY = {
    .id = "ruby", .name = "Ruby",
    .exts = { ".rb", NULL },
    .gsv_lang = "ruby",               // GtkSourceView language id
    .run_cmd = "{ruby} {file}",       // NULL if it can't be run
    .comment = "#", .indent = 2,
    .pairs = "()[]{}\"\"''",           // auto-close pairs, 2 chars each
    .error_parser = NULL,             // or a parser from errors.c
};
...
static const PhotonLanguage *const LANGUAGES[] = { &LANG_LUNAR, &LANG_C, &LANG_PYTHON, &LANG_RUBY, NULL };
```

Then, only if needed:

- **A new `{tool}` in run_cmd**: add a line to `TOOLS` in `runner.c`
  (`{ "ruby", "RUBY", { "ruby", NULL }, FALSE }` = env var, PATH names,
  whether to ask with a file chooser when it isn't found).
- **Error markers**: write `photon_errors_parse_ruby()` in `errors.c`
  (text in, `PhotonError` list out; see the Python one) and add a test
  case with real output to `tests/test_errors.c`.
- **No GtkSourceView highlighting for it**: drop `lang/<id>.lang` in the
  repo (copy `lunar.lang`; style ids are `<id>:<name>` and should `map-to`
  a `def:*` style so every scheme colors them).

Run placeholders: `{file}` `{dir}` `{name}` (file name without extension)
`{exe}` (`<tmp>/photon-<name>`) and any tool in `TOOLS`. Every value is
shell-quoted; the command runs in the file's folder.
