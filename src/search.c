/**
 * @author: Ahmad Odeh
 * @date: 5th Nov, 2026
 *
**/
/*
 * search: find / replace bar and go to line. See search.h.
 *
 *   Find:    [ text          ] [<] [>] [Aa] 3 of 12           [x]
 *   Replace: [ text          ] [Replace] [All]
 */
#include "search.h"

#include <stdlib.h>

#define DATA_CONTEXT "photon-search-context"

struct PhotonSearch {
    GtkWidget              *revealer;    // root widget
    GtkWidget              *find_entry;
    GtkWidget              *replace_entry;
    GtkWidget              *replace_row;
    GtkWidget              *case_button;
    GtkWidget              *count_label;
    GtkSourceSearchSettings *settings;   // shared by every buffer's context
    PhotonSearchTarget      target;
    gpointer                target_data;
    GtkSourceBuffer        *highlighted; // buffer whose matches are showing
};

// ---------------------------------------------------------------------
// Contexts: one per buffer, created on demand, freed with the buffer
// ---------------------------------------------------------------------
static PhotonDocument *
target_doc(PhotonSearch *search) {
    return search->target(search->target_data);
}

static void update_count(PhotonSearch *search);

static void
on_occurrences_changed(GObject *context G_GNUC_UNUSED, GParamSpec *pspec G_GNUC_UNUSED, gpointer user_data) {
    update_count(user_data);
}

static GtkSourceSearchContext *
context_for(PhotonSearch *search, PhotonDocument *doc) {
    GtkSourceBuffer *buffer = photon_document_get_buffer(doc);
    GtkSourceSearchContext *context = g_object_get_data(G_OBJECT(buffer), DATA_CONTEXT);
    if (context == NULL) {
        context = gtk_source_search_context_new(buffer, search->settings);
        g_signal_connect(context, "notify::occurrences-count", G_CALLBACK(on_occurrences_changed), search);
        g_object_set_data_full(G_OBJECT(buffer), DATA_CONTEXT, context, g_object_unref);
    }
    return context;
}

static void
set_highlight(GtkSourceBuffer *buffer, gboolean on) {
    GtkSourceSearchContext *context = g_object_get_data(G_OBJECT(buffer), DATA_CONTEXT);
    if (context != NULL) gtk_source_search_context_set_highlight(context, on);
}

// Highlight matches only in the current tab
static void
highlight_current(PhotonSearch *search) {
    PhotonDocument *doc = target_doc(search);
    GtkSourceBuffer *buffer = doc != NULL ? photon_document_get_buffer(doc) : NULL;
    gboolean visible = gtk_revealer_get_reveal_child(GTK_REVEALER(search->revealer));

    if (search->highlighted != NULL && search->highlighted != buffer) {
        set_highlight(search->highlighted, FALSE);
        g_object_remove_weak_pointer(G_OBJECT(search->highlighted), (gpointer *)&search->highlighted);
        search->highlighted = NULL;
    }
    if (buffer == NULL) return;

    context_for(search, doc);
    set_highlight(buffer, visible);
    if (search->highlighted == NULL) {
        search->highlighted = buffer;
        g_object_add_weak_pointer(G_OBJECT(buffer), (gpointer *)&search->highlighted);
    }
}

// ---------------------------------------------------------------------
// Moving between matches
// ---------------------------------------------------------------------
static void
update_count(PhotonSearch *search) {
    PhotonDocument *doc = target_doc(search);
    const gchar *text = gtk_entry_get_text(GTK_ENTRY(search->find_entry));
    if (doc == NULL || *text == '\0') {
        gtk_label_set_text(GTK_LABEL(search->count_label), "");
        return;
    }

    GtkSourceSearchContext *context = context_for(search, doc);
    gint total = gtk_source_search_context_get_occurrences_count(context);
    GtkTextIter start, end;
    gint position = 0;
    if (gtk_text_buffer_get_selection_bounds(GTK_TEXT_BUFFER(photon_document_get_buffer(doc)), &start, &end)) {
        position = gtk_source_search_context_get_occurrence_position(context, &start, &end);
    }

    gchar *label = NULL;
    if (total < 0) label = g_strdup("");                     // still counting
    else if (total == 0) label = g_strdup("No results");
    else if (position > 0) label = g_strdup_printf("%d of %d", position, total);
    else label = g_strdup_printf("%d found", total);
    gtk_label_set_text(GTK_LABEL(search->count_label), label);
    g_free(label);
}

static void
select_match(PhotonDocument *doc, GtkTextIter *start, GtkTextIter *end) {
    GtkTextBuffer *buffer = GTK_TEXT_BUFFER(photon_document_get_buffer(doc));
    gtk_text_buffer_select_range(buffer, start, end);
    gtk_text_view_scroll_to_iter(GTK_TEXT_VIEW(photon_document_get_view(doc)), start, 0.2, FALSE, 0, 0);
}

// from_start: search from the selection start (typing) instead of its end
static void
find(PhotonSearch *search, gboolean forward, gboolean from_start) {
    PhotonDocument *doc = target_doc(search);
    if (doc == NULL || *gtk_entry_get_text(GTK_ENTRY(search->find_entry)) == '\0') return;

    GtkSourceSearchContext *context = context_for(search, doc);
    GtkTextBuffer *buffer = GTK_TEXT_BUFFER(photon_document_get_buffer(doc));
    GtkTextIter sel_start, sel_end, start, end;
    gboolean wrapped;
    gtk_text_buffer_get_selection_bounds(buffer, &sel_start, &sel_end);

    gboolean found = forward
        ? gtk_source_search_context_forward(context, from_start ? &sel_start : &sel_end, &start, &end, &wrapped)
        : gtk_source_search_context_backward(context, &sel_start, &start, &end, &wrapped);
    if (found) select_match(doc, &start, &end);
    update_count(search);
}

void
photon_search_next(PhotonSearch *search) {
    find(search, TRUE, FALSE);
}

void
photon_search_previous(PhotonSearch *search) {
    find(search, FALSE, FALSE);
}

// ---------------------------------------------------------------------
// Replacing
// ---------------------------------------------------------------------
static void
on_replace(GtkButton *button G_GNUC_UNUSED, gpointer user_data) {
    PhotonSearch *search = user_data;
    PhotonDocument *doc = target_doc(search);
    if (doc == NULL) return;

    GtkSourceSearchContext *context = context_for(search, doc);
    GtkTextIter start, end;
    GtkTextBuffer *buffer = GTK_TEXT_BUFFER(photon_document_get_buffer(doc));

    // Replace the selected match, or go to the next one first
    if (!gtk_text_buffer_get_selection_bounds(buffer, &start, &end)
        || gtk_source_search_context_get_occurrence_position(context, &start, &end) <= 0) {
        photon_search_next(search);
        return;
    }
    const gchar *replacement = gtk_entry_get_text(GTK_ENTRY(search->replace_entry));
    if (gtk_source_search_context_replace(context, &start, &end, replacement, -1, NULL)) {
        photon_search_next(search);
    }
}

static void
on_replace_all(GtkButton *button G_GNUC_UNUSED, gpointer user_data) {
    PhotonSearch *search = user_data;
    PhotonDocument *doc = target_doc(search);
    if (doc == NULL) return;

    const gchar *replacement = gtk_entry_get_text(GTK_ENTRY(search->replace_entry));
    guint count = gtk_source_search_context_replace_all(context_for(search, doc), replacement, -1, NULL);
    gchar *label = g_strdup_printf("Replaced %u", count);
    gtk_label_set_text(GTK_LABEL(search->count_label), label);
    g_free(label);
}

// ---------------------------------------------------------------------
// Bar widgets
// ---------------------------------------------------------------------
static void
on_search_changed(GtkSearchEntry *entry, gpointer user_data) {
    PhotonSearch *search = user_data;
    gtk_source_search_settings_set_search_text(search->settings, gtk_entry_get_text(GTK_ENTRY(entry)));
    find(search, TRUE, TRUE);   // incremental: stay on a match that still fits
}

static void
on_case_toggled(GtkToggleButton *button, gpointer user_data) {
    PhotonSearch *search = user_data;
    gtk_source_search_settings_set_case_sensitive(search->settings, gtk_toggle_button_get_active(button));
    update_count(search);
}

static void
focus_editor(PhotonSearch *search) {
    PhotonDocument *doc = target_doc(search);
    if (doc != NULL) gtk_widget_grab_focus(GTK_WIDGET(photon_document_get_view(doc)));
}

// Enter = next, Shift+Enter = previous, Escape = close
static gboolean
on_entry_key(GtkWidget *entry, GdkEventKey *event, gpointer user_data) {
    PhotonSearch *search = user_data;
    if (event->keyval == GDK_KEY_Escape) {
        photon_search_hide(search);
        return TRUE;
    }
    if (event->keyval == GDK_KEY_Return || event->keyval == GDK_KEY_KP_Enter) {
        if (entry == search->replace_entry) on_replace(NULL, search);
        else if (event->state & GDK_SHIFT_MASK) photon_search_previous(search);
        else photon_search_next(search);
        return TRUE;
    }
    return FALSE;
}

static GtkWidget *
small_button(const gchar *label, const gchar *tooltip, GCallback callback, gpointer data) {
    GtkWidget *button = gtk_button_new_with_label(label);
    gtk_widget_set_tooltip_text(button, tooltip);
    gtk_widget_set_focus_on_click(button, FALSE);
    g_signal_connect_swapped(button, "clicked", callback, data);
    return button;
}

static GtkWidget *
row_label(const gchar *text) {
    GtkWidget *label = gtk_label_new(text);
    gtk_label_set_xalign(GTK_LABEL(label), 1.0f);
    gtk_widget_set_size_request(label, 60, -1);
    return label;
}

static GtkWidget *
build_find_row(PhotonSearch *search) {
    GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    search->find_entry = gtk_search_entry_new();
    gtk_widget_set_size_request(search->find_entry, 280, -1);
    g_signal_connect(search->find_entry, "search-changed", G_CALLBACK(on_search_changed), search);
    g_signal_connect(search->find_entry, "key-press-event", G_CALLBACK(on_entry_key), search);

    search->case_button = gtk_toggle_button_new_with_label("Aa");
    gtk_widget_set_tooltip_text(search->case_button, "Match case");
    gtk_widget_set_focus_on_click(search->case_button, FALSE);
    g_signal_connect(search->case_button, "toggled", G_CALLBACK(on_case_toggled), search);

    search->count_label = gtk_label_new(NULL);

    gtk_box_pack_start(GTK_BOX(row), row_label("Find:"), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(row), search->find_entry, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(row), small_button("<", "Previous (Shift+Enter)", G_CALLBACK(photon_search_previous), search), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(row), small_button(">", "Next (Enter)", G_CALLBACK(photon_search_next), search), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(row), search->case_button, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(row), search->count_label, FALSE, FALSE, 6);
    gtk_box_pack_end(GTK_BOX(row), small_button("×", "Close (Escape)", G_CALLBACK(photon_search_hide), search), FALSE, FALSE, 0);
    return row;
}

static GtkWidget *
build_replace_row(PhotonSearch *search) {
    GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    search->replace_entry = gtk_entry_new();
    gtk_widget_set_size_request(search->replace_entry, 280, -1);
    g_signal_connect(search->replace_entry, "key-press-event", G_CALLBACK(on_entry_key), search);

    GtkWidget *replace = gtk_button_new_with_label("Replace");
    GtkWidget *all = gtk_button_new_with_label("Replace All");
    g_signal_connect(replace, "clicked", G_CALLBACK(on_replace), search);
    g_signal_connect(all, "clicked", G_CALLBACK(on_replace_all), search);

    gtk_box_pack_start(GTK_BOX(row), row_label("Replace:"), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(row), search->replace_entry, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(row), replace, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(row), all, FALSE, FALSE, 0);
    return row;
}

PhotonSearch *
photon_search_new(PhotonSearchTarget target, gpointer user_data) {
    PhotonSearch *search = g_new0(PhotonSearch, 1);
    search->target = target;
    search->target_data = user_data;

    search->settings = gtk_source_search_settings_new();
    gtk_source_search_settings_set_wrap_around(search->settings, TRUE);

    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_style_context_add_class(gtk_widget_get_style_context(box), "photon-search");
    gtk_container_set_border_width(GTK_CONTAINER(box), 4);
    gtk_box_pack_start(GTK_BOX(box), build_find_row(search), FALSE, FALSE, 0);
    search->replace_row = build_replace_row(search);
    gtk_box_pack_start(GTK_BOX(box), search->replace_row, FALSE, FALSE, 0);

    search->revealer = gtk_revealer_new();
    gtk_revealer_set_transition_type(GTK_REVEALER(search->revealer), GTK_REVEALER_TRANSITION_TYPE_NONE);
    gtk_container_add(GTK_CONTAINER(search->revealer), box);
    g_object_ref_sink(search->revealer);
    return search;
}

void
photon_search_free(PhotonSearch *search) {
    if (search == NULL) return;
    if (search->highlighted != NULL) {
        g_object_remove_weak_pointer(G_OBJECT(search->highlighted), (gpointer *)&search->highlighted);
    }
    g_object_unref(search->revealer);
    g_object_unref(search->settings);
    g_free(search);
}

GtkWidget *
photon_search_get_widget(PhotonSearch *search) {
    return search->revealer;
}

// Selected text (one line only) to search for, or NULL
static gchar *
selected_text(PhotonDocument *doc) {
    GtkTextIter start, end;
    GtkTextBuffer *buffer = GTK_TEXT_BUFFER(photon_document_get_buffer(doc));
    if (!gtk_text_buffer_get_selection_bounds(buffer, &start, &end)) return NULL;
    if (gtk_text_iter_get_line(&start) != gtk_text_iter_get_line(&end)) return NULL;
    return gtk_text_buffer_get_text(buffer, &start, &end, FALSE);
}

void
photon_search_show(PhotonSearch *search, gboolean replace) {
    PhotonDocument *doc = target_doc(search);
    gchar *selection = doc != NULL ? selected_text(doc) : NULL;
    if (selection != NULL) gtk_entry_set_text(GTK_ENTRY(search->find_entry), selection);
    g_free(selection);

    gtk_widget_set_visible(search->replace_row, replace);
    gtk_revealer_set_reveal_child(GTK_REVEALER(search->revealer), TRUE);
    gtk_widget_grab_focus(search->find_entry);
    gtk_editable_select_region(GTK_EDITABLE(search->find_entry), 0, -1);

    highlight_current(search);
    update_count(search);
}

void
photon_search_hide(PhotonSearch *search) {
    gtk_revealer_set_reveal_child(GTK_REVEALER(search->revealer), FALSE);
    highlight_current(search);
    focus_editor(search);
}

void
photon_search_document_changed(PhotonSearch *search) {
    highlight_current(search);
    update_count(search);
}

// ---------------------------------------------------------------------
// Go to line
// ---------------------------------------------------------------------
void
photon_search_goto_line(GtkWindow *parent, PhotonDocument *doc) {
    GtkWidget *dialog = gtk_dialog_new_with_buttons("Go to Line", parent,
        GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
        "_Cancel", GTK_RESPONSE_CANCEL, "_Go", GTK_RESPONSE_OK, NULL);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);

    GtkWidget *entry = gtk_entry_new();
    gint lines = gtk_text_buffer_get_line_count(GTK_TEXT_BUFFER(photon_document_get_buffer(doc)));
    gchar *hint = g_strdup_printf("Line (1 - %d), or line:column", lines);
    gtk_entry_set_placeholder_text(GTK_ENTRY(entry), hint);
    gtk_entry_set_activates_default(GTK_ENTRY(entry), TRUE);
    gtk_entry_set_width_chars(GTK_ENTRY(entry), 30);
    g_free(hint);

    GtkWidget *content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    gtk_container_set_border_width(GTK_CONTAINER(content), 8);
    gtk_box_pack_start(GTK_BOX(content), entry, FALSE, FALSE, 0);
    gtk_widget_show_all(dialog);

    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_OK) {
        gint line = 0, column = 0;
        gchar **parts = g_strsplit(gtk_entry_get_text(GTK_ENTRY(entry)), ":", 2);
        if (parts[0] != NULL) line = atoi(parts[0]);
        if (parts[0] != NULL && parts[1] != NULL) column = atoi(parts[1]);
        g_strfreev(parts);
        if (line > 0) photon_document_goto(doc, line, column);
    }
    gtk_widget_destroy(dialog);
}
