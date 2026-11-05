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
#include "sidebar.h"

typedef struct PhotonApp {
    GtkWidget      *window;
    PhotonSidebar  *sidebar;
    PhotonPanel    *panel;
    PhotonDocument *doc;           // the one open file
    guint           autosave_id;   // 0 while auto save is off
} PhotonApp;

typedef struct PhotonAppOptions {
    const gchar *root_dir;      // sidebar root and terminal cwd (required)
    const gchar *file;          // file to open, may not exist yet; NULL = none
    gboolean     show_sidebar;  // TRUE when Photon was started on a folder
} PhotonAppOptions;

PhotonApp *photon_app_new(const PhotonAppOptions *options);
void       photon_app_free(PhotonApp *app);

// Open `path` in the editor; reports errors on stderr.
void photon_app_open_file(PhotonApp *app, const gchar *path);

#endif
