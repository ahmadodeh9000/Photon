/**
 * @author: Ahmad Odeh
 * @date: 5th Nov, 2026
 *
**/
/*
 * platform: every bit of OS-specific code in Photon lives here (and only here).
 * The rest of the app asks these functions instead of using #ifdef __APPLE__.
 */
#ifndef PHOTON_PLATFORM_H
#define PHOTON_PLATFORM_H

#include <glib.h>

// Process-wide OS fixes. Call first thing in main().
void photon_platform_init(void);

// Directory holding the running executable, or NULL if it can't be found.
gchar *photon_platform_executable_dir(void);

// Folder with Photon's data files (lang/, styles/, icons/), or NULL.
// Checks $PHOTON_DATA_DIR, the source tree next to the binary, then the
// install prefix.
gchar *photon_platform_data_dir(void);

// TRUE where GTK's <Primary> modifier is Ctrl (Linux): menu shortcuts
// would then collide with terminal keys like Ctrl+C. FALSE on macOS (Cmd).
gboolean photon_platform_primary_is_control(void);

// The program to start a detached Photon with: the running executable
// (newly allocated), or NULL if it can't be found.
gchar *photon_platform_executable_path(void);

// Start Photon detached from the terminal: its own session, no stdin, no
// output here, and it outlives the terminal. `args` go after the program
// name. On macOS a Photon inside Photon.app starts through LaunchServices
// (`open -a`), so the Dock shows it as Photon with its icon.
gboolean photon_platform_start_detached(gchar **args, GError **error);

// Make the fonts in <data_dir>/fonts (Photon Mono) usable by this process
// only, with nothing installed on the system: CoreText on macOS, fontconfig
// on Linux. Call before gtk_init(). Returns how many font files loaded.
gint photon_platform_load_fonts(const gchar *data_dir);

// Folder for Photon's single-instance socket (newly allocated, created).
// It must be the same for a terminal and for the desktop: on macOS that's
// ~/Library/Caches/photon (apps started by Finder/LaunchServices don't get
// the shell's XDG_* variables), on Linux $XDG_RUNTIME_DIR/photon.
// $PHOTON_RUNTIME_DIR overrides it (tests).
gchar *photon_platform_runtime_dir(void);

// Send this process's stdout/stderr to `log_path` (appending) and read
// stdin from /dev/null. Starts a fresh log once it grows past 1 MB.
void photon_platform_redirect_output(const gchar *log_path);

// Shell to use when VTE can't tell us the user's login shell.
const gchar *photon_platform_fallback_shell(void);

#endif
