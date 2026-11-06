/**
 * @author: Ahmad Odeh
 * @date: 5th Nov, 2026
 *
**/
/*
 * menus: menu bar + shortcuts. See menus.h.
 */
#include "menus.h"
#include "about.h"
#include "editing.h"
#include "fontdialog.h"

#include <vte/vte.h>

static PhotonDocument *
current(gpointer app) {
    return photon_app_current_doc(app);
}

// ---------------------------------------------------------------------
// File
// ---------------------------------------------------------------------
static void on_new(GtkWidget *i G_GNUC_UNUSED, gpointer app)     { photon_app_new_file_dialog(app); }
static void on_open(GtkWidget *i G_GNUC_UNUSED, gpointer app)    { photon_app_open_dialog(app); }
static void on_save(GtkWidget *i G_GNUC_UNUSED, gpointer app)    { photon_app_save(app, current(app)); }
static void on_save_as(GtkWidget *i G_GNUC_UNUSED, gpointer app) { photon_app_save_as(app, current(app)); }
static void on_close(GtkWidget *i G_GNUC_UNUSED, gpointer app)   { photon_app_close_document(app, current(app)); }
static void on_quit(GtkWidget *i G_GNUC_UNUSED, gpointer app)    { photon_app_quit(app); }

static void
on_autosave_toggled(GtkCheckMenuItem *item, gpointer app) {
    photon_app_set_autosave(app, gtk_check_menu_item_get_active(item));
}

// ---------------------------------------------------------------------
// Edit: clipboard actions go to whatever has focus (editor, search
// entries, terminals), not always the editor.
// ---------------------------------------------------------------------
typedef enum { CLIP_CUT, CLIP_COPY, CLIP_PASTE, CLIP_SELECT_ALL } ClipAction;

static void
clipboard(PhotonApp *app, ClipAction action) {
    GtkWidget *focus = gtk_window_get_focus(GTK_WINDOW(app->window));

    if (focus != NULL && VTE_IS_TERMINAL(focus)) {
        VteTerminal *term = VTE_TERMINAL(focus);
        if (action == CLIP_COPY || action == CLIP_CUT) vte_terminal_copy_clipboard_format(term, VTE_FORMAT_TEXT);
        else if (action == CLIP_PASTE) vte_terminal_paste_clipboard(term);
        else vte_terminal_select_all(term);
        return;
    }
    if (focus != NULL && GTK_IS_EDITABLE(focus)) {
        GtkEditable *entry = GTK_EDITABLE(focus);
        if (action == CLIP_CUT) gtk_editable_cut_clipboard(entry);
        else if (action == CLIP_COPY) gtk_editable_copy_clipboard(entry);
        else if (action == CLIP_PASTE) gtk_editable_paste_clipboard(entry);
        else gtk_editable_select_region(entry, 0, -1);
        return;
    }

    GtkSourceView *view = photon_document_get_view(current(app));
    static const gchar *const SIGNALS[] = { "cut-clipboard", "copy-clipboard", "paste-clipboard" };
    if (action == CLIP_SELECT_ALL) g_signal_emit_by_name(view, "select-all", TRUE);
    else g_signal_emit_by_name(view, SIGNALS[action]);
}

static void on_cut(GtkWidget *i G_GNUC_UNUSED, gpointer app)        { clipboard(app, CLIP_CUT); }
static void on_copy(GtkWidget *i G_GNUC_UNUSED, gpointer app)       { clipboard(app, CLIP_COPY); }
static void on_paste(GtkWidget *i G_GNUC_UNUSED, gpointer app)      { clipboard(app, CLIP_PASTE); }
static void on_select_all(GtkWidget *i G_GNUC_UNUSED, gpointer app) { clipboard(app, CLIP_SELECT_ALL); }

static void
on_undo(GtkWidget *item G_GNUC_UNUSED, gpointer app) {
    GtkSourceBuffer *buffer = photon_document_get_buffer(current(app));
    if (gtk_source_buffer_can_undo(buffer)) gtk_source_buffer_undo(buffer);
}

static void
on_redo(GtkWidget *item G_GNUC_UNUSED, gpointer app) {
    GtkSourceBuffer *buffer = photon_document_get_buffer(current(app));
    if (gtk_source_buffer_can_redo(buffer)) gtk_source_buffer_redo(buffer);
}

static void on_find(GtkWidget *i G_GNUC_UNUSED, gpointer app)      { photon_search_show(((PhotonApp *)app)->search, FALSE); }
static void on_replace(GtkWidget *i G_GNUC_UNUSED, gpointer app)   { photon_search_show(((PhotonApp *)app)->search, TRUE); }
static void on_find_next(GtkWidget *i G_GNUC_UNUSED, gpointer app) { photon_search_next(((PhotonApp *)app)->search); }
static void on_find_prev(GtkWidget *i G_GNUC_UNUSED, gpointer app) { photon_search_previous(((PhotonApp *)app)->search); }

static void
on_goto_line(GtkWidget *item G_GNUC_UNUSED, gpointer app) {
    photon_search_goto_line(GTK_WINDOW(((PhotonApp *)app)->window), current(app));
}

static void on_comment(GtkWidget *i G_GNUC_UNUSED, gpointer app)   { photon_editing_toggle_comment(current(app)); }
static void on_duplicate(GtkWidget *i G_GNUC_UNUSED, gpointer app) { photon_editing_duplicate_lines(current(app)); }
static void on_move_up(GtkWidget *i G_GNUC_UNUSED, gpointer app)   { photon_editing_move_lines(current(app), FALSE); }
static void on_move_down(GtkWidget *i G_GNUC_UNUSED, gpointer app) { photon_editing_move_lines(current(app), TRUE); }

// ---------------------------------------------------------------------
// View + Run
// ---------------------------------------------------------------------
static void on_toggle_sidebar(GtkWidget *i G_GNUC_UNUSED, gpointer app)  { photon_sidebar_toggle(((PhotonApp *)app)->sidebar); }
static void on_toggle_terminal(GtkWidget *i G_GNUC_UNUSED, gpointer app) { photon_panel_toggle_terminal(((PhotonApp *)app)->panel); }
static void on_zoom_in(GtkWidget *i G_GNUC_UNUSED, gpointer app)         { photon_app_zoom(app, +1); }
static void on_zoom_out(GtkWidget *i G_GNUC_UNUSED, gpointer app)        { photon_app_zoom(app, -1); }
static void on_zoom_reset(GtkWidget *i G_GNUC_UNUSED, gpointer app)      { photon_app_zoom(app, 0); }
static void on_fonts(GtkWidget *i G_GNUC_UNUSED, gpointer app)           { photon_font_dialog_run(app); }

static void
on_theme_toggled(GtkCheckMenuItem *item, gpointer app) {
    if (gtk_check_menu_item_get_active(item)) photon_app_set_theme(app, g_object_get_data(G_OBJECT(item), "photon-theme"));
}

static void
on_about(GtkWidget *item G_GNUC_UNUSED, gpointer app) {
    photon_about_show(GTK_WINDOW(((PhotonApp *)app)->window));
}

static void on_run(GtkWidget *i G_GNUC_UNUSED, gpointer app)  { photon_runner_run(((PhotonApp *)app)->runner); }
static void on_stop(GtkWidget *i G_GNUC_UNUSED, gpointer app) { photon_runner_stop(((PhotonApp *)app)->runner); }

static void
on_locate_lunar(GtkWidget *item G_GNUC_UNUSED, gpointer app) {
    g_free(photon_runner_locate_lunar(((PhotonApp *)app)->runner));
}

// ---------------------------------------------------------------------
// Building
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

// Extra shortcuts work but aren't shown in the menu
static void
add_shortcut(GtkWidget *item, GtkAccelGroup *accel, const gchar *shortcut, gboolean visible) {
    guint key;
    GdkModifierType mods;
    gtk_accelerator_parse(shortcut, &key, &mods);
    if (key == 0) {
        g_printerr("Bad shortcut: %s\n", shortcut);
        return;
    }
    gtk_widget_add_accelerator(item, "activate", accel, key, mods, visible ? GTK_ACCEL_VISIBLE : 0);
}

// Adds an item with an optional shortcut and returns it. Always write
// shortcuts with "<Primary>": GTK maps it to Cmd on macOS and Ctrl on Linux.
static GtkWidget *
add_item(GtkWidget *menu, GtkAccelGroup *accel, const gchar *label,
         const gchar *shortcut, GCallback callback, PhotonApp *app) {
    GtkWidget *item = gtk_menu_item_new_with_label(label);
    if (shortcut != NULL) add_shortcut(item, accel, shortcut, TRUE);
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
    add_item(menu, accel, "New File...", "<Primary>n",        G_CALLBACK(on_new),     app);
    add_item(menu, accel, "Open...",     "<Primary>o",        G_CALLBACK(on_open),    app);
    add_item(menu, accel, "Save",        "<Primary>s",        G_CALLBACK(on_save),    app);
    add_item(menu, accel, "Save As...",  "<Primary><Shift>s", G_CALLBACK(on_save_as), app);
    add_item(menu, accel, "Close Tab",   "<Primary>w",        G_CALLBACK(on_close),   app);
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
    add_item(menu, accel, "Undo",           "<Primary>z",         G_CALLBACK(on_undo),       app);
    add_item(menu, accel, "Redo",           "<Primary><Shift>z",  G_CALLBACK(on_redo),       app);
    add_separator(menu);
    add_item(menu, accel, "Cut",            "<Primary>x",         G_CALLBACK(on_cut),        app);
    add_item(menu, accel, "Copy",           "<Primary>c",         G_CALLBACK(on_copy),       app);
    add_item(menu, accel, "Paste",          "<Primary>v",         G_CALLBACK(on_paste),      app);
    add_item(menu, accel, "Select All",     "<Primary>a",         G_CALLBACK(on_select_all), app);
    add_separator(menu);
    add_item(menu, accel, "Find...",        "<Primary>f",         G_CALLBACK(on_find),       app);
    add_item(menu, accel, "Replace...",     "<Primary><Alt>f",    G_CALLBACK(on_replace),    app);
    add_item(menu, accel, "Find Next",      "<Primary>g",         G_CALLBACK(on_find_next),  app);
    add_item(menu, accel, "Find Previous",  "<Primary><Shift>g",  G_CALLBACK(on_find_prev),  app);
    add_item(menu, accel, "Go to Line...",  "<Primary>l",         G_CALLBACK(on_goto_line),  app);
    add_separator(menu);
    add_item(menu, accel, "Toggle Comment", "<Primary>slash",     G_CALLBACK(on_comment),    app);
    add_item(menu, accel, "Duplicate Line", "<Primary>d",         G_CALLBACK(on_duplicate),  app);
    add_item(menu, accel, "Move Line Up",   "<Alt>Up",            G_CALLBACK(on_move_up),    app);
    add_item(menu, accel, "Move Line Down", "<Alt>Down",          G_CALLBACK(on_move_down),  app);
}

static void
build_view_menu(PhotonApp *app, GtkWidget *menu_bar, GtkAccelGroup *accel) {
    GtkWidget *menu = add_menu(menu_bar, "View");
    add_item(menu, accel, "Toggle Sidebar", "<Primary>b", G_CALLBACK(on_toggle_sidebar), app);
    GtkWidget *terminal = add_item(menu, accel, "Toggle Terminal", "<Primary>grave", G_CALLBACK(on_toggle_terminal), app);
    add_shortcut(terminal, accel, "<Primary>t", FALSE);
    add_separator(menu);

    GtkWidget *zoom_in = add_item(menu, accel, "Zoom In", "<Primary>plus", G_CALLBACK(on_zoom_in), app);
    add_shortcut(zoom_in, accel, "<Primary>equal", FALSE);     // "+" without Shift
    add_shortcut(zoom_in, accel, "<Primary>KP_Add", FALSE);
    GtkWidget *zoom_out = add_item(menu, accel, "Zoom Out", "<Primary>minus", G_CALLBACK(on_zoom_out), app);
    add_shortcut(zoom_out, accel, "<Primary>KP_Subtract", FALSE);
    add_item(menu, accel, "Actual Size", "<Primary>0", G_CALLBACK(on_zoom_reset), app);
    add_item(menu, accel, "Fonts...", "<Primary>comma", G_CALLBACK(on_fonts), app);
    add_separator(menu);

    // View > Theme > (radio items, one per theme in theme.c)
    GtkWidget *themes = gtk_menu_new();
    GtkWidget *themes_item = gtk_menu_item_new_with_label("Theme");
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(themes_item), themes);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), themes_item);

    GSList *group = NULL;
    const gchar *active = photon_theme_get_id(app->theme);
    for (const gchar *const *id = photon_theme_ids(); *id != NULL; id++) {
        GtkWidget *item = gtk_radio_menu_item_new_with_label(group, photon_theme_name(*id));
        group = gtk_radio_menu_item_get_group(GTK_RADIO_MENU_ITEM(item));
        g_object_set_data(G_OBJECT(item), "photon-theme", (gpointer)*id);
        gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item), g_strcmp0(*id, active) == 0);
        g_signal_connect(item, "toggled", G_CALLBACK(on_theme_toggled), app);
        gtk_menu_shell_append(GTK_MENU_SHELL(themes), item);
    }
}

static void
build_run_menu(PhotonApp *app, GtkWidget *menu_bar, GtkAccelGroup *accel) {
    GtkWidget *menu = add_menu(menu_bar, "Run");
    GtkWidget *run = add_item(menu, accel, "Run", "<Primary>r", G_CALLBACK(on_run), app);
    add_shortcut(run, accel, "F5", FALSE);
    add_item(menu, accel, "Stop", "<Primary>period", G_CALLBACK(on_stop), app);
    add_separator(menu);
    add_item(menu, accel, "Locate Lunar...", NULL, G_CALLBACK(on_locate_lunar), app);
}

GtkWidget *
photon_menus_build(PhotonApp *app) {
    GtkAccelGroup *accel = gtk_accel_group_new();
    gtk_window_add_accel_group(GTK_WINDOW(app->window), accel);

    GtkWidget *menu_bar = gtk_menu_bar_new();
    build_file_menu(app, menu_bar, accel);
    build_edit_menu(app, menu_bar, accel);
    build_view_menu(app, menu_bar, accel);
    build_run_menu(app, menu_bar, accel);
    GtkWidget *help = add_menu(menu_bar, "Help");
    add_item(help, accel, "About Photon", NULL, G_CALLBACK(on_about), app);

    g_object_unref(accel); // the window holds its own reference
    return menu_bar;
}
