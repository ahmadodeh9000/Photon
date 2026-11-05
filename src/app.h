/**
 * @author: Ahmad Odeh
 * @date: 5th Nov, 2026
 *
**/
/*
 * app: the main window. Owns the layout (sidebar | search bar / tabs over
 * panel | status bar), the open documents' lifecycle (new, open, save,
 * close, quit, with "unsaved changes?" questions), the session (files to
 * reopen) and the zoom level. Every callback gets the PhotonApp* instead
 * of reaching for globals. Menus and shortcuts live in menus.c.
 */
#ifndef PHOTON_APP_H
#define PHOTON_APP_H

#include <gtk/gtk.h>

#include "completion.h"
#include "document.h"
#include "panel.h"
#include "runner.h"
#include "search.h"
#include "settings.h"
#include "sidebar.h"
#include "statusbar.h"
#include "tabs.h"
#include "theme.h"

struct PhotonApp {
    GtkWidget        *window;
    PhotonSettings   *settings;
    PhotonTheme      *theme;        // owned by main()
    PhotonSidebar    *sidebar;
    PhotonTabs       *tabs;
    PhotonSearch     *search;
    PhotonCompletion *completion;
    PhotonPanel      *panel;
    PhotonStatusbar  *statusbar;
    PhotonRunner     *runner;
    GtkWidget        *title_strip;  // Win95 title bar above the tabs (some themes)
    GtkWidget        *title_label;
    gchar            *root_dir;     // sidebar root; default folder for Save As
    gboolean          use_session;  // read/write the list of open files
    gboolean          restoring;    // reopening the session: don't save it meanwhile
    gboolean          quitting;     // don't replace the last closed tab
    guint             autosave_id;  // 0 while auto save is off
};

typedef struct PhotonAppOptions {
    const gchar *root_dir;      // sidebar root and terminal cwd (required)
    const gchar *file;          // file to open, may not exist yet; NULL = none
    gboolean     show_sidebar;  // TRUE when Photon was started on a folder
    PhotonTheme *theme;         // required, outlives the app
    gboolean     use_session;   // reopen last files / remember open files
    gboolean     read_only;     // never write settings.ini (smoke tests)
    const gchar *theme_id;      // theme for this run (--theme); NULL = the saved one
} PhotonAppOptions;

PhotonApp *photon_app_new(const PhotonAppOptions *options);
void       photon_app_free(PhotonApp *app);

PhotonDocument *photon_app_current_doc(PhotonApp *app);   // never NULL

// Documents
PhotonDocument *photon_app_new_document(PhotonApp *app, const PhotonLanguage *language);
void            photon_app_new_file_dialog(PhotonApp *app);  // asks the language, Lunar first
gboolean        photon_app_open_file(PhotonApp *app, const gchar *path);  // switches if already open
void            photon_app_open_dialog(PhotonApp *app);
gboolean        photon_app_save(PhotonApp *app, PhotonDocument *doc);     // asks a name if untitled
gboolean        photon_app_save_as(PhotonApp *app, PhotonDocument *doc);
gboolean        photon_app_close_document(PhotonApp *app, PhotonDocument *doc);  // FALSE = cancelled

// Asks about unsaved files, remembers the session, then quits.
// Returns FALSE if the user cancelled.
gboolean photon_app_quit(PhotonApp *app);

void photon_app_set_autosave(PhotonApp *app, gboolean on);

// Switch theme (see theme.c) and remember it.
void photon_app_set_theme(PhotonApp *app, const gchar *theme_id);

// Zoom the code font by `steps` points; 0 resets it.
void photon_app_zoom(PhotonApp *app, gint steps);

// Run errors: open the file of the first error if needed, mark the lines
// in every open document, jump to it and show its message.
void photon_app_show_errors(PhotonApp *app, GPtrArray *errors);
void photon_app_clear_errors(PhotonApp *app);

#endif
