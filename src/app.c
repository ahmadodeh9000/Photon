/**
 * @author: Ahmad Odeh
 * @date: 5th Nov, 2026
 *
**/
/*
 * app: main window, layout, menus and shortcuts. See app.h.
 */
#include "app.h"
#include "editing.h"

#define AUTOSAVE_SECONDS 30

// ---------------------------------------------------------------------
// Title + file actions
// ---------------------------------------------------------------------
static void
update_title(PhotonApp *app) {
    const gchar *path = photon_document_get_path(app->doc);
    if (path == NULL) {
        gtk_window_set_title(GTK_WINDOW(app->window), "Photon");
        return;
    }

    gchar *basename = g_path_get_basename(path);
    gchar *title = g_strdup_printf("%s - Photon", basename);
    gtk_window_set_title(GTK_WINDOW(app->window), title);
    g_free(basename);
    g_free(title);
}

// Errors go to the status bar and stderr
static void
report(PhotonApp *app, const gchar *what, GError *error) {
    gchar *msg = g_strdup_printf("%s: %s", what, error->message);
    g_printerr("%s\n", msg);
    photon_statusbar_set_message(app->statusbar, msg, TRUE);
    g_free(msg);
    g_error_free(error);
}

gboolean
photon_app_open_file(PhotonApp *app, const gchar *path) {
    GError *error = NULL;
    if (!photon_document_load(app->doc, path, &error)) {
        report(app, "Failed to load file", error);
        return FALSE;
    }
    update_title(app);
    photon_statusbar_set_message(app->statusbar, NULL, FALSE);
    return TRUE;
}

static gboolean
save_to(PhotonApp *app, const gchar *path) {
    GError *error = NULL;
    if (!photon_document_save_as(app->doc, path, &error)) {
        report(app, "Failed to save file", error);
        return FALSE;
    }
    update_title(app);
    return TRUE;
}

// Native dialogs: NSOpenPanel/NSSavePanel on macOS, GTK/portal dialogs on Linux
static gchar *
choose_file(PhotonApp *app, GtkFileChooserAction action) {
    gboolean saving = (action == GTK_FILE_CHOOSER_ACTION_SAVE);
    GtkFileChooserNative *native = gtk_file_chooser_native_new(
        saving ? "Save File" : "Open File", GTK_WINDOW(app->window), action,
        saving ? "_Save" : "_Open", "_Cancel");

    const gchar *current = photon_document_get_path(app->doc);
    if (saving) {
        gtk_file_chooser_set_do_overwrite_confirmation(GTK_FILE_CHOOSER(native), TRUE);
        if (current != NULL) gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(native), current);
    }

    gchar *filename = NULL;
    if (gtk_native_dialog_run(GTK_NATIVE_DIALOG(native)) == GTK_RESPONSE_ACCEPT) {
        filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(native));
    }
    g_object_unref(native);
    return filename;
}

static gboolean
save_as(PhotonApp *app) {
    gchar *filename = choose_file(app, GTK_FILE_CHOOSER_ACTION_SAVE);
    gboolean ok = filename != NULL && save_to(app, filename);
    g_free(filename);
    return ok;
}

gboolean
photon_app_save(PhotonApp *app) {
    const gchar *path = photon_document_get_path(app->doc);
    if (path == NULL) return save_as(app);
    return save_to(app, path);   // safe: the document copies the path before replacing it
}

void
photon_app_show_errors(PhotonApp *app, GPtrArray *errors) {
    const PhotonError *first = photon_errors_first(errors);

    // The error may be in another file (an imported Lunar module, a header)
    if (first != NULL) {
        const gchar *path = photon_document_get_path(app->doc);
        gchar *current = path != NULL ? g_canonicalize_filename(path, NULL) : NULL;
        if (g_strcmp0(current, first->path) != 0 && g_file_test(first->path, G_FILE_TEST_IS_REGULAR)) {
            photon_app_open_file(app, first->path);
        }
        g_free(current);
    }

    photon_document_show_errors(app->doc, errors);
    if (first == NULL) return;   // warnings only: marked, no jump

    gchar *base = g_path_get_basename(first->path);
    gchar *msg = g_strdup_printf("%s:%d: %s", base, first->line, first->message);
    photon_statusbar_set_message(app->statusbar, msg, TRUE);
    photon_document_goto(app->doc, first->line, first->column);
    g_free(base);
    g_free(msg);
}

// ---------------------------------------------------------------------
// Menu callbacks (user_data is always the PhotonApp*)
// ---------------------------------------------------------------------
static void
on_open(GtkWidget *item G_GNUC_UNUSED, gpointer user_data) {
    PhotonApp *app = user_data;
    gchar *filename = choose_file(app, GTK_FILE_CHOOSER_ACTION_OPEN);
    if (filename != NULL) photon_app_open_file(app, filename);
    g_free(filename);
}

static void
on_save(GtkWidget *item G_GNUC_UNUSED, gpointer user_data) {
    photon_app_save(user_data);
}

static void
on_save_as(GtkWidget *item G_GNUC_UNUSED, gpointer user_data) {
    save_as(user_data);
}

static gboolean
autosave_tick(gpointer user_data) {
    PhotonApp *app = user_data;
    if (photon_document_get_path(app->doc) != NULL && photon_document_is_dirty(app->doc)) {
        photon_app_save(app);
    }
    return G_SOURCE_CONTINUE;
}

static void
on_autosave_toggled(GtkCheckMenuItem *item, gpointer user_data) {
    PhotonApp *app = user_data;
    gboolean on = gtk_check_menu_item_get_active(item);

    if (on && app->autosave_id == 0) {
        app->autosave_id = g_timeout_add_seconds(AUTOSAVE_SECONDS, autosave_tick, app);
    } else if (!on && app->autosave_id != 0) {
        g_source_remove(app->autosave_id);
        app->autosave_id = 0;
    }
}

static void
on_quit(GtkWidget *item G_GNUC_UNUSED, gpointer user_data G_GNUC_UNUSED) {
    gtk_main_quit();
}

static void
on_undo(GtkWidget *item G_GNUC_UNUSED, gpointer user_data) {
    GtkSourceBuffer *buffer = photon_document_get_buffer(((PhotonApp *)user_data)->doc);
    if (gtk_source_buffer_can_undo(buffer)) gtk_source_buffer_undo(buffer);
}

static void
on_redo(GtkWidget *item G_GNUC_UNUSED, gpointer user_data) {
    GtkSourceBuffer *buffer = photon_document_get_buffer(((PhotonApp *)user_data)->doc);
    if (gtk_source_buffer_can_redo(buffer)) gtk_source_buffer_redo(buffer);
}

// Clipboard actions are keybinding signals on the view
static void
emit_on_view(PhotonApp *app, const gchar *signal) {
    g_signal_emit_by_name(photon_document_get_view(app->doc), signal);
}

static void on_cut(GtkWidget *i G_GNUC_UNUSED, gpointer app)   { emit_on_view(app, "cut-clipboard"); }
static void on_copy(GtkWidget *i G_GNUC_UNUSED, gpointer app)  { emit_on_view(app, "copy-clipboard"); }
static void on_paste(GtkWidget *i G_GNUC_UNUSED, gpointer app) { emit_on_view(app, "paste-clipboard"); }

static void
on_select_all(GtkWidget *item G_GNUC_UNUSED, gpointer user_data) {
    PhotonApp *app = user_data;
    g_signal_emit_by_name(photon_document_get_view(app->doc), "select-all", TRUE);
}

static void
on_toggle_comment(GtkWidget *item G_GNUC_UNUSED, gpointer user_data) {
    photon_editing_toggle_comment(((PhotonApp *)user_data)->doc);
}

static void
on_toggle_sidebar(GtkWidget *item G_GNUC_UNUSED, gpointer user_data) {
    photon_sidebar_toggle(((PhotonApp *)user_data)->sidebar);
}

static void
on_toggle_terminal(GtkWidget *item G_GNUC_UNUSED, gpointer user_data) {
    photon_panel_toggle_terminal(((PhotonApp *)user_data)->panel);
}

static void
on_run(GtkWidget *item G_GNUC_UNUSED, gpointer user_data) {
    photon_runner_run(((PhotonApp *)user_data)->runner);
}

static void
on_stop(GtkWidget *item G_GNUC_UNUSED, gpointer user_data) {
    photon_runner_stop(((PhotonApp *)user_data)->runner);
}

static void
on_locate_lunar(GtkWidget *item G_GNUC_UNUSED, gpointer user_data) {
    g_free(photon_runner_locate_lunar(((PhotonApp *)user_data)->runner));
}

static void
on_sidebar_open(const gchar *path, gpointer user_data) {
    photon_app_open_file(user_data, path);
}

// ---------------------------------------------------------------------
// Menu bar
// ---------------------------------------------------------------------

// Adds a submenu titled `title` to the menu bar and returns it
static GtkWidget *
add_menu(GtkWidget *menu_bar, const gchar *title) {
    GtkWidget *menu = gtk_menu_new();
    GtkWidget *item = gtk_menu_item_new_with_label(title);
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(item), menu);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu_bar), item);
    return menu;
}

static void
add_shortcut(GtkWidget *item, GtkAccelGroup *accel, const gchar *shortcut) {
    guint key;
    GdkModifierType mods;
    gtk_accelerator_parse(shortcut, &key, &mods);
    if (key != 0) gtk_widget_add_accelerator(item, "activate", accel, key, mods, GTK_ACCEL_VISIBLE);
}

// Adds an item with an optional shortcut and returns it. Always write
// shortcuts with "<Primary>": GTK maps it to Cmd on macOS and Ctrl on Linux.
static GtkWidget *
add_item(GtkWidget *menu, GtkAccelGroup *accel, const gchar *label,
         const gchar *shortcut, GCallback callback, PhotonApp *app) {
    GtkWidget *item = gtk_menu_item_new_with_label(label);
    if (shortcut != NULL) add_shortcut(item, accel, shortcut);
    g_signal_connect(item, "activate", callback, app);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
    return item;
}

static void
add_separator(GtkWidget *menu) {
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), gtk_separator_menu_item_new());
}

static void
build_file_menu(PhotonApp *app, GtkWidget *menu_bar, GtkAccelGroup *accel) {
    GtkWidget *menu = add_menu(menu_bar, "File");
    add_item(menu, accel, "Open",       "<Primary>o",        G_CALLBACK(on_open),    app);
    add_item(menu, accel, "Save",       "<Primary>s",        G_CALLBACK(on_save),    app);
    add_item(menu, accel, "Save As...", "<Primary><Shift>s", G_CALLBACK(on_save_as), app);
    add_separator(menu);

    GtkWidget *autosave = gtk_check_menu_item_new_with_label("Auto Save (every 30s)");
    g_signal_connect(autosave, "toggled", G_CALLBACK(on_autosave_toggled), app);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), autosave);   // off by default

    add_separator(menu);
    add_item(menu, accel, "Quit", "<Primary>q", G_CALLBACK(on_quit), app);
}

static void
build_edit_menu(PhotonApp *app, GtkWidget *menu_bar, GtkAccelGroup *accel) {
    GtkWidget *menu = add_menu(menu_bar, "Edit");
    add_item(menu, accel, "Undo",           "<Primary>z",        G_CALLBACK(on_undo),           app);
    add_item(menu, accel, "Redo",           "<Primary><Shift>z", G_CALLBACK(on_redo),           app);
    add_separator(menu);
    add_item(menu, accel, "Cut",            "<Primary>x",        G_CALLBACK(on_cut),            app);
    add_item(menu, accel, "Copy",           "<Primary>c",        G_CALLBACK(on_copy),           app);
    add_item(menu, accel, "Paste",          "<Primary>v",        G_CALLBACK(on_paste),          app);
    add_separator(menu);
    add_item(menu, accel, "Select All",     "<Primary>a",        G_CALLBACK(on_select_all),     app);
    add_item(menu, accel, "Toggle Comment", "<Primary>slash",    G_CALLBACK(on_toggle_comment), app);
}

static void
build_view_menu(PhotonApp *app, GtkWidget *menu_bar, GtkAccelGroup *accel) {
    GtkWidget *menu = add_menu(menu_bar, "View");
    add_item(menu, accel, "Toggle Sidebar",  "<Primary>b", G_CALLBACK(on_toggle_sidebar),  app);
    add_item(menu, accel, "Toggle Terminal", "<Primary>t", G_CALLBACK(on_toggle_terminal), app);
}

static void
build_run_menu(PhotonApp *app, GtkWidget *menu_bar, GtkAccelGroup *accel) {
    GtkWidget *menu = add_menu(menu_bar, "Run");
    GtkWidget *run = add_item(menu, accel, "Run", "<Primary>r", G_CALLBACK(on_run), app);
    add_shortcut(run, accel, "F5");
    add_item(menu, accel, "Stop", "<Primary>period", G_CALLBACK(on_stop), app);
    add_separator(menu);
    add_item(menu, accel, "Locate Lunar...", NULL, G_CALLBACK(on_locate_lunar), app);
}

static GtkWidget *
build_menu_bar(PhotonApp *app) {
    GtkAccelGroup *accel = gtk_accel_group_new();
    gtk_window_add_accel_group(GTK_WINDOW(app->window), accel);

    GtkWidget *menu_bar = gtk_menu_bar_new();
    build_file_menu(app, menu_bar, accel);
    build_edit_menu(app, menu_bar, accel);
    build_view_menu(app, menu_bar, accel);
    build_run_menu(app, menu_bar, accel);

    g_object_unref(accel); // the window holds its own reference
    return menu_bar;
}

// ---------------------------------------------------------------------
// Layout:  [ sidebar | menu bar / (editor over panel) ]
//          [ status bar                                ]
// ---------------------------------------------------------------------
static GtkWidget *
build_layout(PhotonApp *app) {
    GtkWidget *editor_split = gtk_paned_new(GTK_ORIENTATION_VERTICAL);
    gtk_paned_pack1(GTK_PANED(editor_split), photon_document_get_widget(app->doc), TRUE, FALSE);
    gtk_paned_pack2(GTK_PANED(editor_split), photon_panel_get_widget(app->panel), FALSE, FALSE);
    gtk_paned_set_position(GTK_PANED(editor_split), 420);

    GtkWidget *right = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_box_pack_start(GTK_BOX(right), build_menu_bar(app), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(right), editor_split, TRUE, TRUE, 0);

    GtkWidget *main_split = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_paned_pack1(GTK_PANED(main_split), photon_sidebar_get_widget(app->sidebar), FALSE, FALSE);
    gtk_paned_pack2(GTK_PANED(main_split), right, TRUE, FALSE);
    gtk_paned_set_position(GTK_PANED(main_split), 220);

    GtkWidget *outer = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_box_pack_start(GTK_BOX(outer), main_split, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(outer), photon_statusbar_get_widget(app->statusbar), FALSE, FALSE, 0);
    return outer;
}

static void
open_initial_file(PhotonApp *app, const gchar *file) {
    if (file == NULL) return;

    if (g_file_test(file, G_FILE_TEST_EXISTS)) {
        photon_app_open_file(app, file);
    } else {
        // Not on disk yet: remember the name so Save (not Save As) writes there
        photon_document_set_path(app->doc, file);
    }
}

PhotonApp *
photon_app_new(const PhotonAppOptions *options) {
    PhotonApp *app = g_new0(PhotonApp, 1);

    app->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_default_size(GTK_WINDOW(app->window), 1000, 650);
    gtk_window_set_icon_name(GTK_WINDOW(app->window), "photon");
    g_signal_connect(app->window, "delete-event", G_CALLBACK(gtk_main_quit), NULL);

    app->settings = photon_settings_load();
    app->statusbar = photon_statusbar_new();
    app->doc = photon_document_new();
    app->panel = photon_panel_new(options->root_dir);
    app->runner = photon_runner_new(app);
    app->sidebar = photon_sidebar_new(options->root_dir, on_sidebar_open, app);

    gtk_container_add(GTK_CONTAINER(app->window), build_layout(app));
    open_initial_file(app, options->file);
    update_title(app);

    // Show everything, then hide what starts hidden
    gtk_widget_show_all(app->window);
    if (!options->show_sidebar) gtk_widget_hide(photon_sidebar_get_widget(app->sidebar));
    gtk_widget_hide(photon_panel_get_widget(app->panel));

    return app;
}

void
photon_app_free(PhotonApp *app) {
    if (app == NULL) return;
    if (app->autosave_id != 0) g_source_remove(app->autosave_id);
    // Modules drop their refs first (disconnecting their signals); the
    // window then destroys the widgets for real.
    photon_runner_free(app->runner);
    photon_sidebar_free(app->sidebar);
    photon_panel_free(app->panel);
    photon_document_free(app->doc);
    photon_statusbar_free(app->statusbar);
    gtk_widget_destroy(app->window);
    photon_settings_free(app->settings);
    g_free(app);
}
