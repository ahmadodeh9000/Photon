/**
 * @author: Ahmad Odeh
 * @date: 5th Nov, 2026
 *
**/
/*
 * app: main window, documents, session. See app.h.
 */
#include "app.h"
#include "menus.h"
#include "platform.h"

#include <vte/vte.h>

#define AUTOSAVE_SECONDS 30
#define SESSION_GROUP    "session"
#define EDITOR_GROUP     "editor"
#define FONTS_GROUP      "fonts"

enum { ANSWER_SAVE = 1, ANSWER_DISCARD = 2 };   // GTK_RESPONSE_* are negative

// ---------------------------------------------------------------------
// Title + status bar
// ---------------------------------------------------------------------
// Takes the document because during a tab switch the notebook may not
// report the new current page yet
static void
update_title(PhotonApp *app, PhotonDocument *doc) {
    gchar *name = photon_document_get_title(doc);
    gchar *title = g_strdup_printf("%s - Photon", name);
    gtk_window_set_title(GTK_WINDOW(app->window), title);

    // The strip shows the full path, like "Photon - [C:\TC\MAIN.C]"
    const gchar *path = photon_document_get_path(doc);
    gchar *strip = g_strdup_printf("Photon - [%s]", path != NULL ? path : name);
    gtk_label_set_text(GTK_LABEL(app->title_label), strip);

    g_free(strip);
    g_free(title);
    g_free(name);
}

static void
update_position(PhotonApp *app, PhotonDocument *doc) {
    GtkTextBuffer *buffer = GTK_TEXT_BUFFER(photon_document_get_buffer(doc));
    GtkTextIter iter;
    gtk_text_buffer_get_iter_at_mark(buffer, &iter, gtk_text_buffer_get_insert(buffer));
    photon_statusbar_set_position(app->statusbar, gtk_text_iter_get_line(&iter) + 1,
                                  gtk_text_iter_get_line_offset(&iter) + 1);
}

static void
update_status(PhotonApp *app, PhotonDocument *doc) {
    const PhotonLanguage *language = photon_document_get_language(doc);
    photon_statusbar_set_language(app->statusbar, language != NULL ? language->name : "Plain Text");
    update_position(app, doc);
}

static void
update_window(PhotonApp *app, PhotonDocument *doc) {
    if (doc == NULL) return;
    update_title(app, doc);
    update_status(app, doc);
}

// Any buffer's cursor moved; only the current one is shown
static void
on_cursor_moved(GObject *buffer, GParamSpec *pspec G_GNUC_UNUSED, gpointer user_data) {
    PhotonApp *app = user_data;
    PhotonDocument *doc = photon_tabs_current(app->tabs);
    if (doc != NULL && G_OBJECT(photon_document_get_buffer(doc)) == buffer) update_position(app, doc);
}

static void
report(PhotonApp *app, const gchar *what, GError *error) {
    gchar *msg = g_strdup_printf("%s: %s", what, error->message);
    g_printerr("%s\n", msg);
    photon_statusbar_set_message(app->statusbar, msg, TRUE);
    g_free(msg);
    g_error_free(error);
}

// ---------------------------------------------------------------------
// Session: the open files, reopened on the next launch
// ---------------------------------------------------------------------
static void
save_session(PhotonApp *app) {
    if (!app->use_session || app->restoring || app->quitting) return;

    GPtrArray *files = g_ptr_array_new();
    gint current = 0;
    PhotonDocument *active = photon_app_current_doc(app);
    GList *docs = photon_tabs_list(app->tabs);
    for (GList *l = docs; l != NULL; l = l->next) {
        const gchar *path = photon_document_get_path(l->data);
        if (path == NULL) continue;
        if (l->data == active) current = (gint)files->len;
        g_ptr_array_add(files, (gpointer)path);
    }
    g_ptr_array_add(files, NULL);

    photon_settings_set_strv(app->settings, SESSION_GROUP, "files", (const gchar *const *)files->pdata);
    photon_settings_set_int(app->settings, SESSION_GROUP, "current", current);
    g_list_free(docs);
    g_ptr_array_free(files, TRUE);
}

static void
restore_session(PhotonApp *app) {
    gchar **files = photon_settings_get_strv(app->settings, SESSION_GROUP, "files");
    if (files == NULL) return;

    PhotonDocument *select = NULL;
    gint current = photon_settings_get_int(app->settings, SESSION_GROUP, "current", 0);
    app->restoring = TRUE;
    for (gint i = 0; files[i] != NULL; i++) {
        if (!g_file_test(files[i], G_FILE_TEST_IS_REGULAR)) continue;   // deleted since
        if (photon_app_open_file(app, files[i]) && i == current) select = photon_app_current_doc(app);
    }
    if (select != NULL) photon_tabs_select(app->tabs, select);
    app->restoring = FALSE;
    g_strfreev(files);
}

// ---------------------------------------------------------------------
// Adding and removing documents
// ---------------------------------------------------------------------
static void
add_document(PhotonApp *app, PhotonDocument *doc) {
    photon_theme_attach_buffer(app->theme, photon_document_get_buffer(doc));
    photon_document_set_marker_colors(doc, photon_theme_marker_colors(app->theme));
    photon_completion_attach(app->completion, doc);
    g_signal_connect(photon_document_get_buffer(doc), "notify::cursor-position", G_CALLBACK(on_cursor_moved), app);
    photon_tabs_add(app->tabs, doc);
}

static void
remove_document(PhotonApp *app, PhotonDocument *doc) {
    gchar *path = g_strdup(photon_document_get_path(doc));
    g_signal_handlers_disconnect_by_data(photon_document_get_buffer(doc), app);
    photon_completion_detach(app->completion, doc);
    photon_tabs_close(app->tabs, doc);
    photon_remote_document_closed(app->remote, path);   // releases `photon -w`
    g_free(path);

    // Always keep one tab, so "the current document" exists
    if (!app->quitting && photon_tabs_count(app->tabs) == 0) photon_app_new_document(app, photon_languages_find("lunar"));
}

PhotonDocument *
photon_app_current_doc(PhotonApp *app) {
    return photon_tabs_current(app->tabs);
}

PhotonDocument *
photon_app_new_document(PhotonApp *app, const PhotonLanguage *language) {
    PhotonDocument *doc = photon_document_new();
    photon_document_set_language(doc, language);
    add_document(app, doc);
    gtk_widget_grab_focus(GTK_WIDGET(photon_document_get_view(doc)));
    return doc;
}

gboolean
photon_app_open_file(PhotonApp *app, const gchar *path) {
    PhotonDocument *open = photon_tabs_find(app->tabs, path);
    if (open != NULL) {
        photon_tabs_select(app->tabs, open);
        return TRUE;
    }

    // Reuse an empty "Untitled" tab instead of piling up blank ones
    PhotonDocument *current = photon_tabs_count(app->tabs) > 0 ? photon_app_current_doc(app) : NULL;
    gboolean reuse = current != NULL && photon_document_is_blank(current);
    PhotonDocument *doc = reuse ? current : photon_document_new();

    GError *error = NULL;
    if (!photon_document_load(doc, path, &error)) {
        report(app, "Failed to load file", error);
        if (!reuse) photon_document_free(doc);
        return FALSE;
    }
    if (!reuse) add_document(app, doc);

    update_window(app, doc);
    photon_statusbar_set_message(app->statusbar, NULL, FALSE);
    save_session(app);
    return TRUE;
}

// ---------------------------------------------------------------------
// Dialogs
// ---------------------------------------------------------------------

// Native dialogs: NSOpenPanel/NSSavePanel on macOS, GTK/portal dialogs on Linux
static gchar *
choose_file(PhotonApp *app, PhotonDocument *doc, GtkFileChooserAction action) {
    gboolean saving = (action == GTK_FILE_CHOOSER_ACTION_SAVE);
    GtkFileChooserNative *native = gtk_file_chooser_native_new(
        saving ? "Save File" : "Open File", GTK_WINDOW(app->window), action,
        saving ? "_Save" : "_Open", "_Cancel");
    GtkFileChooser *chooser = GTK_FILE_CHOOSER(native);

    const gchar *current = doc != NULL ? photon_document_get_path(doc) : NULL;
    if (saving) {
        gtk_file_chooser_set_do_overwrite_confirmation(chooser, TRUE);
        if (current != NULL) {
            gtk_file_chooser_set_filename(chooser, current);
        } else {
            gchar *name = photon_document_get_title(doc);   // "Untitled.lunar"
            gtk_file_chooser_set_current_folder(chooser, app->root_dir);
            gtk_file_chooser_set_current_name(chooser, name);
            g_free(name);
        }
    }

    gchar *filename = NULL;
    if (gtk_native_dialog_run(GTK_NATIVE_DIALOG(native)) == GTK_RESPONSE_ACCEPT) {
        filename = gtk_file_chooser_get_filename(chooser);
    }
    g_object_unref(native);
    return filename;
}

void
photon_app_open_dialog(PhotonApp *app) {
    gchar *filename = choose_file(app, NULL, GTK_FILE_CHOOSER_ACTION_OPEN);
    if (filename != NULL) photon_app_open_file(app, filename);
    g_free(filename);
}

// "New File" asks which language; Lunar is selected, so Enter takes it
void
photon_app_new_file_dialog(PhotonApp *app) {
    GtkWidget *dialog = gtk_dialog_new_with_buttons("New File", GTK_WINDOW(app->window),
        GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
        "_Cancel", GTK_RESPONSE_CANCEL, "_Create", GTK_RESPONSE_OK, NULL);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);

    GtkWidget *combo = gtk_combo_box_text_new();
    const PhotonLanguage *const *all = photon_languages_all();
    for (int i = 0; all[i] != NULL; i++) gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(combo), all[i]->id, all[i]->name);
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(combo), "", "Plain Text");
    gtk_combo_box_set_active(GTK_COMBO_BOX(combo), 0);

    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_container_set_border_width(GTK_CONTAINER(box), 10);
    gtk_box_pack_start(GTK_BOX(box), gtk_label_new("Language:"), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), combo, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog))), box, FALSE, FALSE, 0);
    gtk_widget_show_all(dialog);
    gtk_widget_grab_focus(gtk_dialog_get_widget_for_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK));

    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_OK) {
        const gchar *id = gtk_combo_box_get_active_id(GTK_COMBO_BOX(combo));
        photon_app_new_document(app, photon_languages_find(id));
    }
    gtk_widget_destroy(dialog);
}

// Save / Don't Save / Cancel
static gint
ask_unsaved(PhotonApp *app, const gchar *question, const gchar *detail, const gchar *save_label) {
    GtkWidget *dialog = gtk_message_dialog_new(GTK_WINDOW(app->window), GTK_DIALOG_MODAL,
        GTK_MESSAGE_WARNING, GTK_BUTTONS_NONE, "%s", question);
    gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dialog), "%s", detail);
    gtk_dialog_add_buttons(GTK_DIALOG(dialog), "Do_n't Save", ANSWER_DISCARD,
                           "_Cancel", GTK_RESPONSE_CANCEL, save_label, ANSWER_SAVE, NULL);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), ANSWER_SAVE);
    gint answer = gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
    return answer;
}

// ---------------------------------------------------------------------
// Saving, closing, quitting
// ---------------------------------------------------------------------
static gboolean
save_to(PhotonApp *app, PhotonDocument *doc, const gchar *path) {
    GError *error = NULL;
    gboolean is_new = !g_file_test(path, G_FILE_TEST_EXISTS);
    if (!photon_document_save_as(doc, path, &error)) {
        report(app, "Failed to save file", error);
        return FALSE;
    }
    if (is_new) photon_sidebar_refresh(app->sidebar);   // show the new file
    return TRUE;
}

gboolean
photon_app_save_as(PhotonApp *app, PhotonDocument *doc) {
    gchar *filename = choose_file(app, doc, GTK_FILE_CHOOSER_ACTION_SAVE);
    gboolean ok = filename != NULL && save_to(app, doc, filename);
    g_free(filename);
    if (ok) save_session(app);
    return ok;
}

gboolean
photon_app_save(PhotonApp *app, PhotonDocument *doc) {
    const gchar *path = photon_document_get_path(doc);
    if (path == NULL) return photon_app_save_as(app, doc);
    return save_to(app, doc, path);   // safe: the document copies the path before replacing it
}

gboolean
photon_app_close_document(PhotonApp *app, PhotonDocument *doc) {
    if (photon_document_is_dirty(doc)) {
        photon_tabs_select(app->tabs, doc);
        gchar *name = photon_document_get_title(doc);
        gchar *question = g_strdup_printf("Save changes to \"%s\" before closing?", name);
        gint answer = ask_unsaved(app, question, "Your changes will be lost if you don't save them.", "_Save");
        g_free(question);
        g_free(name);

        if (answer == ANSWER_SAVE && !photon_app_save(app, doc)) return FALSE;
        if (answer != ANSWER_SAVE && answer != ANSWER_DISCARD) return FALSE;   // cancelled
    }
    remove_document(app, doc);
    save_session(app);
    return TRUE;
}

static GList *
dirty_documents(PhotonApp *app) {
    GList *dirty = NULL;
    GList *docs = photon_tabs_list(app->tabs);
    for (GList *l = docs; l != NULL; l = l->next) {
        if (photon_document_is_dirty(l->data)) dirty = g_list_append(dirty, l->data);
    }
    g_list_free(docs);
    return dirty;
}

gboolean
photon_app_quit(PhotonApp *app) {
    GList *dirty = dirty_documents(app);
    guint count = g_list_length(dirty);
    gboolean ok = TRUE;

    if (count > 0) {
        gchar *question = count == 1
            ? g_strdup("1 file has unsaved changes. Save it before quitting?")
            : g_strdup_printf("%u files have unsaved changes. Save them before quitting?", count);
        gint answer = ask_unsaved(app, question, "Your changes will be lost if you don't save them.",
                                  count == 1 ? "_Save" : "_Save All");
        g_free(question);

        if (answer == ANSWER_SAVE) {
            for (GList *l = dirty; ok && l != NULL; l = l->next) {
                photon_tabs_select(app->tabs, l->data);
                ok = photon_app_save(app, l->data);
            }
        } else if (answer != ANSWER_DISCARD) {
            ok = FALSE;
        }
    }
    g_list_free(dirty);
    if (!ok) return FALSE;

    save_session(app);
    app->quitting = TRUE;
    gtk_main_quit();
    return TRUE;
}

static gboolean
on_delete_event(GtkWidget *window G_GNUC_UNUSED, GdkEvent *event G_GNUC_UNUSED, gpointer user_data) {
    photon_app_quit(user_data);
    return TRUE;   // the window is destroyed in photon_app_free()
}

// ---------------------------------------------------------------------
// Auto save, zoom, errors
// ---------------------------------------------------------------------
static gboolean
autosave_tick(gpointer user_data) {
    PhotonApp *app = user_data;
    GList *docs = photon_tabs_list(app->tabs);
    for (GList *l = docs; l != NULL; l = l->next) {
        if (photon_document_get_path(l->data) != NULL && photon_document_is_dirty(l->data)) {
            photon_app_save(app, l->data);
        }
    }
    g_list_free(docs);
    return G_SOURCE_CONTINUE;
}

void
photon_app_set_autosave(PhotonApp *app, gboolean on) {
    if (on && app->autosave_id == 0) {
        app->autosave_id = g_timeout_add_seconds(AUTOSAVE_SECONDS, autosave_tick, app);
    } else if (!on && app->autosave_id != 0) {
        g_source_remove(app->autosave_id);
        app->autosave_id = 0;
    }
}

void
photon_app_set_theme(PhotonApp *app, const gchar *theme_id) {
    if (!photon_theme_set(app->theme, theme_id)) return;
    photon_settings_set_string(app->settings, EDITOR_GROUP, "theme", theme_id);

    gtk_widget_set_visible(app->title_strip, photon_theme_has_title_strip(app->theme));
    GList *docs = photon_tabs_list(app->tabs);
    for (GList *l = docs; l != NULL; l = l->next) {
        photon_document_set_marker_colors(l->data, photon_theme_marker_colors(app->theme));
    }
    g_list_free(docs);
}

void
photon_app_zoom(PhotonApp *app, gint steps) {
    photon_theme_set_zoom(app->theme, steps == 0 ? 0 : photon_theme_get_zoom(app->theme) + steps);
    photon_settings_set_int(app->settings, FONTS_GROUP, "zoom", photon_theme_get_zoom(app->theme));
}

void
photon_app_set_fonts(PhotonApp *app, const PhotonFonts *fonts, gboolean save) {
    photon_theme_set_fonts(app->theme, fonts);
    if (!save) return;
    photon_settings_set_string(app->settings, FONTS_GROUP, "code_family", fonts->code_family != NULL ? fonts->code_family : "");
    photon_settings_set_int(app->settings, FONTS_GROUP, "code_size", fonts->code_points);
    photon_settings_set_string(app->settings, FONTS_GROUP, "ui_family", fonts->ui_family != NULL ? fonts->ui_family : "");
    photon_settings_set_int(app->settings, FONTS_GROUP, "ui_size", fonts->ui_points);
}

// "" in settings.ini = the theme's suggestion
static gchar *
saved_family(PhotonApp *app, const gchar *key) {
    gchar *family = photon_settings_get_string(app->settings, FONTS_GROUP, key);
    if (family != NULL && *family == '\0') g_clear_pointer(&family, g_free);
    return family;
}

static void
restore_fonts(PhotonApp *app) {
    PhotonFonts fonts = {
        .code_family = saved_family(app, "code_family"),
        .ui_family = saved_family(app, "ui_family"),
        .ui_points = photon_settings_get_int(app->settings, FONTS_GROUP, "ui_size", 0),
    };
    // Before the font picker, [editor] font_size held the zoomed code size
    gint old_size = photon_settings_get_int(app->settings, EDITOR_GROUP, "font_size", 0);
    fonts.code_points = photon_settings_get_int(app->settings, FONTS_GROUP, "code_size", old_size);

    photon_theme_set_fonts(app->theme, &fonts);
    photon_theme_set_zoom(app->theme, photon_settings_get_int(app->settings, FONTS_GROUP, "zoom", 0));
    photon_fonts_clear(&fonts);
}

void
photon_app_clear_errors(PhotonApp *app) {
    GList *docs = photon_tabs_list(app->tabs);
    for (GList *l = docs; l != NULL; l = l->next) photon_document_clear_errors(l->data);
    g_list_free(docs);
}

void
photon_app_show_errors(PhotonApp *app, GPtrArray *errors) {
    const PhotonError *first = photon_errors_first(errors);

    // The error may be in another file (an imported Lunar module, a header)
    if (first != NULL && g_file_test(first->path, G_FILE_TEST_IS_REGULAR)) {
        photon_app_open_file(app, first->path);
    }

    GList *docs = photon_tabs_list(app->tabs);
    for (GList *l = docs; l != NULL; l = l->next) photon_document_show_errors(l->data, errors);
    g_list_free(docs);
    if (first == NULL) return;   // warnings only: marked, no jump

    gchar *base = g_path_get_basename(first->path);
    gchar *msg = g_strdup_printf("%s:%d: %s", base, first->line, first->message);
    photon_statusbar_set_message(app->statusbar, msg, TRUE);
    photon_document_goto(photon_app_current_doc(app), first->line, first->column);
    g_free(base);
    g_free(msg);
}

// ---------------------------------------------------------------------
// Callbacks from tabs, status bar, sidebar
// ---------------------------------------------------------------------
static void
on_tab_switched(PhotonDocument *doc, gpointer user_data) {
    PhotonApp *app = user_data;
    update_window(app, doc);
    if (photon_tabs_current(app->tabs) == doc) {
        photon_search_document_changed(app->search);
        save_session(app);   // remember which tab was in front
    }
}

static void
on_tab_close(PhotonDocument *doc, gpointer user_data) {
    photon_app_close_document(user_data, doc);
}

static void
on_doc_changed(PhotonDocument *doc, PhotonDocChange what, gpointer user_data) {
    PhotonApp *app = user_data;
    if (what & PHOTON_DOC_LANGUAGE) photon_completion_attach(app->completion, doc);
    if (doc == photon_tabs_current(app->tabs)) update_window(app, doc);
}

static void
on_language_picked(const PhotonLanguage *language, gpointer user_data) {
    PhotonApp *app = user_data;
    photon_document_set_language(photon_app_current_doc(app), language);
}

static void
on_sidebar_open(const gchar *path, gpointer user_data) {
    photon_app_open_file(user_data, path);
}

static PhotonDocument *
search_target(gpointer user_data) {
    return photon_app_current_doc(user_data);
}

// ---------------------------------------------------------------------
// Keys for the terminals on Linux
//
// <Primary> is Ctrl on Linux, so menu shortcuts would steal Ctrl+C, Ctrl+D,
// Ctrl+R, Ctrl+W... from the shell. While a terminal has focus it gets keys
// first, except the panel toggles. Ctrl+Shift+C / V copy and paste, like
// every Linux terminal. On macOS (Cmd) nothing changes.
// ---------------------------------------------------------------------
static gboolean
on_window_key_press(GtkWidget *window, GdkEventKey *event, gpointer user_data G_GNUC_UNUSED) {
    GtkWidget *focus = gtk_window_get_focus(GTK_WINDOW(window));
    if (!photon_platform_primary_is_control() || !VTE_IS_TERMINAL(focus)) return FALSE;

    GdkModifierType mods = event->state & gtk_accelerator_get_default_mod_mask();
    if (!(mods & GDK_CONTROL_MASK)) return FALSE;

    guint key = gdk_keyval_to_lower(event->keyval);
    if (key == GDK_KEY_grave || key == GDK_KEY_t) return FALSE;   // toggle terminal: menu handles it
    if (mods == (GDK_CONTROL_MASK | GDK_SHIFT_MASK) && key == GDK_KEY_c) {
        vte_terminal_copy_clipboard_format(VTE_TERMINAL(focus), VTE_FORMAT_TEXT);
        return TRUE;
    }
    if (mods == (GDK_CONTROL_MASK | GDK_SHIFT_MASK) && key == GDK_KEY_v) {
        vte_terminal_paste_clipboard(VTE_TERMINAL(focus));
        return TRUE;
    }
    return gtk_window_propagate_key_event(GTK_WINDOW(window), event);
}

// ---------------------------------------------------------------------
// Layout:  [ menu bar                                      ]
//          [ sidebar | search bar / title strip + tabs     ]
//          [         |            over the bottom panel    ]
//          [ status bar                                    ]
// ---------------------------------------------------------------------
// Gradient strip with the file's path, above the tabs
static GtkWidget *
build_title_strip(PhotonApp *app) {
    app->title_label = gtk_label_new("Photon");
    gtk_label_set_xalign(GTK_LABEL(app->title_label), 0.0f);
    gtk_label_set_ellipsize(GTK_LABEL(app->title_label), PANGO_ELLIPSIZE_START);
    app->title_strip = gtk_event_box_new();
    gtk_container_add(GTK_CONTAINER(app->title_strip), app->title_label);
    gtk_style_context_add_class(gtk_widget_get_style_context(app->title_strip), "photon-titlebar");
    return app->title_strip;
}

static GtkWidget *
build_layout(PhotonApp *app) {
    GtkWidget *editor = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_box_pack_start(GTK_BOX(editor), build_title_strip(app), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(editor), photon_tabs_get_widget(app->tabs), TRUE, TRUE, 0);

    GtkWidget *editor_split = gtk_paned_new(GTK_ORIENTATION_VERTICAL);
    gtk_paned_pack1(GTK_PANED(editor_split), editor, TRUE, FALSE);
    gtk_paned_pack2(GTK_PANED(editor_split), photon_panel_get_widget(app->panel), FALSE, FALSE);
    gtk_paned_set_position(GTK_PANED(editor_split), 400);

    GtkWidget *right = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_box_pack_start(GTK_BOX(right), photon_search_get_widget(app->search), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(right), editor_split, TRUE, TRUE, 0);

    GtkWidget *main_split = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_paned_pack1(GTK_PANED(main_split), photon_sidebar_get_widget(app->sidebar), FALSE, FALSE);
    gtk_paned_pack2(GTK_PANED(main_split), right, TRUE, FALSE);
    gtk_paned_set_position(GTK_PANED(main_split), 220);

    GtkWidget *outer = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_box_pack_start(GTK_BOX(outer), photon_menus_build(app), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(outer), main_split, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(outer), photon_statusbar_get_widget(app->statusbar), FALSE, FALSE, 0);
    return outer;
}

// Open a file, or a tab for one that doesn't exist yet (Save writes there)
static void
open_or_create(PhotonApp *app, const gchar *path) {
    if (g_file_test(path, G_FILE_TEST_EXISTS)) {
        photon_app_open_file(app, path);
        return;
    }
    PhotonDocument *open = photon_tabs_find(app->tabs, path);
    if (open != NULL) {
        photon_tabs_select(app->tabs, open);
        return;
    }
    PhotonDocument *doc = photon_app_new_document(app, NULL);
    photon_document_set_path(doc, path);
}

static void
open_initial_files(PhotonApp *app, const PhotonAppOptions *options) {
    if (app->use_session) restore_session(app);
    for (gint i = 0; options->files != NULL && options->files[i] != NULL; i++) {
        open_or_create(app, options->files[i]);
    }
    if (photon_tabs_count(app->tabs) == 0) photon_app_new_document(app, photon_languages_find("lunar"));
}

// ---------------------------------------------------------------------
// Requests from the `photon` command (remote.c)
// ---------------------------------------------------------------------
static void
remote_open(const gchar *path, gboolean run, gpointer user_data) {
    PhotonApp *app = user_data;
    open_or_create(app, path);
    if (run) photon_runner_run(app->runner);
}

static void
remote_close(const gchar *path, gpointer user_data) {
    PhotonApp *app = user_data;
    PhotonDocument *doc = photon_tabs_find(app->tabs, path);
    if (doc != NULL) photon_app_close_document(app, doc);
}

static gchar **
remote_list(gpointer user_data) {
    PhotonApp *app = user_data;
    GPtrArray *paths = g_ptr_array_new();
    GList *docs = photon_tabs_list(app->tabs);
    for (GList *l = docs; l != NULL; l = l->next) {
        const gchar *path = photon_document_get_path(l->data);
        if (path != NULL) g_ptr_array_add(paths, g_strdup(path));
    }
    g_list_free(docs);
    g_ptr_array_add(paths, NULL);
    return (gchar **)g_ptr_array_free(paths, FALSE);
}

static void
remote_present(gpointer user_data) {
    PhotonApp *app = user_data;
    gtk_window_present(GTK_WINDOW(app->window));   // also activates the app on macOS
}

static void
start_remote(PhotonApp *app) {
    PhotonRemoteHandlers handlers = {
        .open = remote_open, .close = remote_close, .list = remote_list,
        .present = remote_present, .user_data = app,
    };
    app->remote = photon_remote_serve(&handlers);   // NULL: another Photon serves; fine
}

static void
create_modules(PhotonApp *app, const PhotonAppOptions *options) {
    PhotonTabsCallbacks tab_callbacks = {
        .close_requested = on_tab_close,
        .switched = on_tab_switched,
        .doc_changed = on_doc_changed,
        .user_data = app,
    };

    app->settings = photon_settings_load(options->read_only);
    app->statusbar = photon_statusbar_new(on_language_picked, app);
    app->completion = photon_completion_new();
    app->tabs = photon_tabs_new(&tab_callbacks);
    app->search = photon_search_new(search_target, app);
    app->panel = photon_panel_new(options->root_dir, app->theme);
    app->runner = photon_runner_new(app);
    app->sidebar = photon_sidebar_new(options->root_dir, on_sidebar_open, app);
}

PhotonApp *
photon_app_new(const PhotonAppOptions *options) {
    PhotonApp *app = g_new0(PhotonApp, 1);
    app->theme = options->theme;
    app->root_dir = g_strdup(options->root_dir);
    app->use_session = options->use_session;

    app->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_default_size(GTK_WINDOW(app->window), 1100, 700);
    gtk_window_set_icon_name(GTK_WINDOW(app->window), "photon");
    g_signal_connect(app->window, "delete-event", G_CALLBACK(on_delete_event), app);
    g_signal_connect(app->window, "key-press-event", G_CALLBACK(on_window_key_press), app);

    create_modules(app, options);
    restore_fonts(app);

    // Before the menus are built, so View > Theme shows the right one
    gchar *theme_id = options->theme_id != NULL ? g_strdup(options->theme_id)
                    : photon_settings_get_string(app->settings, EDITOR_GROUP, "theme");
    if (theme_id == NULL || !photon_theme_set(app->theme, theme_id)) photon_theme_set(app->theme, PHOTON_THEME_DEFAULT);
    g_free(theme_id);
    gtk_container_add(GTK_CONTAINER(app->window), build_layout(app));
    open_initial_files(app, options);
    if (options->serve_remote) start_remote(app);

    // Show everything, then hide what starts hidden
    gtk_widget_show_all(app->window);
    if (!options->show_sidebar) gtk_widget_hide(photon_sidebar_get_widget(app->sidebar));
    gtk_widget_hide(photon_panel_get_widget(app->panel));
    photon_search_hide(app->search);
    gtk_widget_set_visible(app->title_strip, photon_theme_has_title_strip(app->theme));
    update_window(app, photon_app_current_doc(app));
    return app;
}

void
photon_app_free(PhotonApp *app) {
    if (app == NULL) return;
    app->quitting = TRUE;
    photon_app_set_autosave(app, FALSE);
    photon_remote_free(app->remote);   // waiting `photon -w` commands return
    app->remote = NULL;

    // Modules drop their refs first (disconnecting their signals); the
    // window then destroys the widgets for real.
    GList *docs = photon_tabs_list(app->tabs);
    for (GList *l = docs; l != NULL; l = l->next) remove_document(app, l->data);
    g_list_free(docs);

    photon_runner_free(app->runner);
    photon_tabs_free(app->tabs);
    photon_search_free(app->search);
    photon_completion_free(app->completion);
    photon_sidebar_free(app->sidebar);
    photon_panel_free(app->panel);
    photon_statusbar_free(app->statusbar);
    gtk_widget_destroy(app->window);
    photon_settings_free(app->settings);
    g_free(app->root_dir);
    g_free(app);
}
