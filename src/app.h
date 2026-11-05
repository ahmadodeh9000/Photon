/**
 * @author: Ahmad Odeh
 * @date: 5th Nov, 2026
 *
**/
/*
 * app: the main window. Owns the layout (sidebar | editor over panel), the
 * menus and keyboard shortcuts, and the window title. Every callback gets
 * the PhotonApp* instead of reaching for globals.
 */
#ifndef PHOTON_APP_H
#define PHOTON_APP_H

#include <gtk/gtk.h>

#include "document.h"
#include "panel.h"
#include "runner.h"
#include "settings.h"
#include "sidebar.h"
#include "statusbar.h"

struct PhotonApp {
    GtkWidget       *window;
    PhotonSettings  *settings;
    PhotonSidebar   *sidebar;
    PhotonPanel     *panel;
    PhotonStatusbar *statusbar;
    PhotonRunner    *runner;
    PhotonDocument  *doc;           // the one open file
    guint            autosave_id;   // 0 while auto save is off
};

typedef struct PhotonAppOptions {
    const gchar *root_dir;      // sidebar root and terminal cwd (required)
    const gchar *file;          // file to open, may not exist yet; NULL = none
    gboolean     show_sidebar;  // TRUE when Photon was started on a folder
} PhotonAppOptions;

PhotonApp *photon_app_new(const PhotonAppOptions *options);
void       photon_app_free(PhotonApp *app);

// Open `path` in the editor. Returns FALSE (and says why) if it can't.
gboolean photon_app_open_file(PhotonApp *app, const gchar *path);

// Save the current document, asking for a name if it has none.
// Returns FALSE if cancelled or the write failed.
gboolean photon_app_save(PhotonApp *app);

// Show run errors: open the file of the first error if needed, mark the
// lines, jump to it and put its message in the status bar.
void photon_app_show_errors(PhotonApp *app, GPtrArray *errors);

#endif
