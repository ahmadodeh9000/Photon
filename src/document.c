/**
 * @author: Ahmad Odeh
 * @date: 5th Nov, 2026
 *
**/
/*
 * document: one open file (buffer + view + path + markers). See document.h.
 */
#include "document.h"
#include "editing.h"

#define MARK_ERROR    "photon-error"
#define MARK_WARNING  "photon-warning"
#define TAG_ERROR     "photon-error"
#define TAG_WARNING   "photon-warning"
#define DATA_MESSAGE  "photon-message"

struct PhotonDocument {
    GtkSourceBuffer      *buffer;
    GtkWidget            *view;
    GtkWidget            *scroll;      // root widget, holds the view
    gchar                *path;        // NULL until loaded or saved
    const PhotonLanguage *language;    // registry entry, NULL if unknown
    gboolean              has_errors;  // markers are showing

    PhotonDocumentListener listener;
    gpointer               listener_data;
};

static void
notify(PhotonDocument *doc, PhotonDocChange what) {
    if (doc->listener != NULL) doc->listener(doc, what, doc->listener_data);
}

static void
on_modified_changed(GtkTextBuffer *buffer G_GNUC_UNUSED, gpointer user_data) {
    notify(user_data, PHOTON_DOC_DIRTY);
}

// ---------------------------------------------------------------------
// Error marker look: a filled circle in the gutter, tinted line, squiggle
// ---------------------------------------------------------------------
static GdkPixbuf *
make_dot(const gchar *color) {
    const int size = 12;
    GdkRGBA rgba;
    gdk_rgba_parse(&rgba, color);

    cairo_surface_t *surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, size, size);
    cairo_t *cr = cairo_create(surface);
    gdk_cairo_set_source_rgba(cr, &rgba);
    cairo_arc(cr, size / 2.0, size / 2.0, size / 2.0 - 1, 0, 2 * G_PI);
    cairo_fill(cr);
    cairo_destroy(cr);

    GdkPixbuf *pixbuf = gdk_pixbuf_get_from_surface(surface, 0, 0, size, size);
    cairo_surface_destroy(surface);
    return pixbuf;
}

static gchar *
on_mark_tooltip(GtkSourceMarkAttributes *attrs G_GNUC_UNUSED, GtkSourceMark *mark, gpointer user_data G_GNUC_UNUSED) {
    return g_strdup(g_object_get_data(G_OBJECT(mark), DATA_MESSAGE));
}

static void
add_mark_category(GtkSourceView *view, const gchar *category, const gchar *dot, const gchar *tint) {
    GtkSourceMarkAttributes *attrs = gtk_source_mark_attributes_new();
    GdkPixbuf *pixbuf = make_dot(dot);
    GdkRGBA background;
    gdk_rgba_parse(&background, tint);

    gtk_source_mark_attributes_set_pixbuf(attrs, pixbuf);
    gtk_source_mark_attributes_set_background(attrs, &background);
    g_signal_connect(attrs, "query-tooltip-text", G_CALLBACK(on_mark_tooltip), NULL);
    gtk_source_view_set_mark_attributes(view, category, attrs, 10);

    g_object_unref(pixbuf);
    g_object_unref(attrs);
}

static void
setup_markers(PhotonDocument *doc) {
    GdkRGBA red, orange;
    gdk_rgba_parse(&red, "#F44747");
    gdk_rgba_parse(&orange, "#D7BA7D");
    gtk_text_buffer_create_tag(GTK_TEXT_BUFFER(doc->buffer), TAG_ERROR,
        "underline", PANGO_UNDERLINE_ERROR, "underline-rgba", &red, NULL);
    gtk_text_buffer_create_tag(GTK_TEXT_BUFFER(doc->buffer), TAG_WARNING,
        "underline", PANGO_UNDERLINE_ERROR, "underline-rgba", &orange, NULL);

    add_mark_category(GTK_SOURCE_VIEW(doc->view), MARK_ERROR,   "#F44747", "rgba(244,71,71,0.18)");
    add_mark_category(GTK_SOURCE_VIEW(doc->view), MARK_WARNING, "#D7BA7D", "rgba(215,186,125,0.15)");
    gtk_source_view_set_show_line_marks(GTK_SOURCE_VIEW(doc->view), TRUE);
}

static void
on_buffer_changed(GtkTextBuffer *buffer G_GNUC_UNUSED, gpointer user_data) {
    PhotonDocument *doc = user_data;
    if (doc->has_errors) photon_document_clear_errors(doc);
}

// ---------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------
PhotonDocument *
photon_document_new(void) {
    PhotonDocument *doc = g_new0(PhotonDocument, 1);

    // No language yet: it's picked per file in set_path()
    doc->buffer = gtk_source_buffer_new(NULL);   // the app styles it (theme.c)

    doc->view = gtk_source_view_new_with_buffer(doc->buffer);
    GtkSourceView *view = GTK_SOURCE_VIEW(doc->view);
    gtk_source_view_set_show_line_numbers(view, TRUE);
    gtk_source_view_set_auto_indent(view, TRUE);
    gtk_source_view_set_highlight_current_line(view, TRUE);
    gtk_source_view_set_smart_home_end(view, GTK_SOURCE_SMART_HOME_END_BEFORE);
    gtk_source_view_set_insert_spaces_instead_of_tabs(view, TRUE);   // Tab inserts spaces
    gtk_source_view_set_indent_width(view, 4);
    gtk_source_view_set_tab_width(view, 4);
    gtk_source_buffer_set_highlight_matching_brackets(doc->buffer, TRUE);

    doc->scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(doc->scroll),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_container_add(GTK_CONTAINER(doc->scroll), doc->view);
    g_object_ref_sink(doc->scroll); // we own it until free, whoever packs it

    setup_markers(doc);
    g_signal_connect(doc->buffer, "changed", G_CALLBACK(on_buffer_changed), doc);
    g_signal_connect(doc->buffer, "modified-changed", G_CALLBACK(on_modified_changed), doc);
    photon_editing_attach(doc);
    return doc;
}

void
photon_document_free(PhotonDocument *doc) {
    if (doc == NULL) return;
    doc->listener = NULL;
    g_signal_handlers_disconnect_by_data(doc->buffer, doc);
    g_signal_handlers_disconnect_by_data(doc->view, doc);
    g_object_unref(doc->scroll);
    g_object_unref(doc->buffer);
    g_free(doc->path);
    g_free(doc);
}

// Highlighting + indent from a registry entry (NULL = plain text, 4 spaces)
static void
use_language(PhotonDocument *doc, const PhotonLanguage *language, GtkSourceLanguage *highlighting) {
    GtkSourceView *view = GTK_SOURCE_VIEW(doc->view);
    guint indent = language != NULL ? language->indent : 4;
    gboolean changed = language != doc->language;

    doc->language = language;
    gtk_source_buffer_set_language(doc->buffer, highlighting);
    gtk_source_view_set_tab_width(view, indent);
    gtk_source_view_set_indent_width(view, (gint)indent);
    if (changed) notify(doc, PHOTON_DOC_LANGUAGE);
}

void
photon_document_set_language(PhotonDocument *doc, const PhotonLanguage *language) {
    use_language(doc, language, language != NULL ? photon_languages_get(language->gsv_lang) : NULL);
}

void
photon_document_set_path(PhotonDocument *doc, const gchar *path) {
    // Duplicate BEFORE freeing: `path` may be doc->path itself
    gchar *copy = g_strdup(path);
    gboolean renamed = g_strcmp0(copy, doc->path) != 0;
    g_free(doc->path);
    doc->path = copy;

    // "Save As x.lunar" switches highlighting. A name with no known
    // extension keeps the language the file already had.
    const PhotonLanguage *language = photon_languages_for_file(doc->path);
    GtkSourceLanguage *highlighting = photon_languages_highlighting_for_file(doc->path);
    if (language != NULL || highlighting != NULL) use_language(doc, language, highlighting);

    if (renamed) notify(doc, PHOTON_DOC_PATH);
}

void
photon_document_set_listener(PhotonDocument *doc, PhotonDocumentListener listener, gpointer user_data) {
    doc->listener = listener;
    doc->listener_data = user_data;
}

gchar *
photon_document_get_title(PhotonDocument *doc) {
    if (doc->path != NULL) return g_path_get_basename(doc->path);
    if (doc->language != NULL) return g_strconcat("Untitled", doc->language->exts[0], NULL);
    return g_strdup("Untitled");
}

gboolean
photon_document_is_blank(PhotonDocument *doc) {
    return doc->path == NULL && !photon_document_is_dirty(doc)
        && gtk_text_buffer_get_char_count(GTK_TEXT_BUFFER(doc->buffer)) == 0;
}

// ---------------------------------------------------------------------
// Load / save
// ---------------------------------------------------------------------
gboolean
photon_document_load(PhotonDocument *doc, const gchar *path, GError **error) {
    gchar *contents;
    gsize length;

    if (!g_file_get_contents(path, &contents, &length, error)) return FALSE;

    // Binary files would trip gtk_text_buffer's UTF-8 assertion
    if (!g_utf8_validate(contents, length, NULL)) {
        g_set_error(error, G_CONVERT_ERROR, G_CONVERT_ERROR_ILLEGAL_SEQUENCE,
                    "Refusing to open '%s': not a valid UTF-8 text file (likely binary).", path);
        g_free(contents);
        return FALSE;
    }

    photon_document_set_path(doc, path);

    gtk_source_buffer_begin_not_undoable_action(doc->buffer);
    gtk_text_buffer_set_text(GTK_TEXT_BUFFER(doc->buffer), contents, (gint)length);
    gtk_source_buffer_end_not_undoable_action(doc->buffer);
    gtk_text_buffer_set_modified(GTK_TEXT_BUFFER(doc->buffer), FALSE);

    GtkTextIter start;
    gtk_text_buffer_get_start_iter(GTK_TEXT_BUFFER(doc->buffer), &start);
    gtk_text_buffer_place_cursor(GTK_TEXT_BUFFER(doc->buffer), &start);

    g_free(contents);
    return TRUE;
}

gboolean
photon_document_save_as(PhotonDocument *doc, const gchar *path, GError **error) {
    GtkTextIter start, end;
    gtk_text_buffer_get_bounds(GTK_TEXT_BUFFER(doc->buffer), &start, &end);
    gchar *text = gtk_text_buffer_get_text(GTK_TEXT_BUFFER(doc->buffer), &start, &end, FALSE);

    gboolean ok = g_file_set_contents(path, text, -1, error);
    g_free(text);
    if (!ok) return FALSE;

    photon_document_set_path(doc, path);
    gtk_text_buffer_set_modified(GTK_TEXT_BUFFER(doc->buffer), FALSE);
    return TRUE;
}

gboolean
photon_document_save(PhotonDocument *doc, GError **error) {
    if (doc->path == NULL) {
        g_set_error_literal(error, G_FILE_ERROR, G_FILE_ERROR_INVAL, "Document has no file name yet");
        return FALSE;
    }
    return photon_document_save_as(doc, doc->path, error);
}

// ---------------------------------------------------------------------
// Error markers + navigation
// ---------------------------------------------------------------------
void
photon_document_clear_errors(PhotonDocument *doc) {
    GtkTextBuffer *buffer = GTK_TEXT_BUFFER(doc->buffer);
    GtkTextIter start, end;
    gtk_text_buffer_get_bounds(buffer, &start, &end);

    gtk_text_buffer_remove_tag_by_name(buffer, TAG_ERROR, &start, &end);
    gtk_text_buffer_remove_tag_by_name(buffer, TAG_WARNING, &start, &end);
    gtk_source_buffer_remove_source_marks(doc->buffer, &start, &end, MARK_ERROR);
    gtk_source_buffer_remove_source_marks(doc->buffer, &start, &end, MARK_WARNING);
    doc->has_errors = FALSE;
}

// Iter at the start of a 1-based line, clamped to the buffer
static void
line_start(GtkTextBuffer *buffer, gint line, GtkTextIter *iter) {
    gint last = gtk_text_buffer_get_line_count(buffer);
    gtk_text_buffer_get_iter_at_line(buffer, iter, CLAMP(line, 1, last) - 1);
}

static void
mark_line(PhotonDocument *doc, const PhotonError *error) {
    GtkTextBuffer *buffer = GTK_TEXT_BUFFER(doc->buffer);
    GtkTextIter start, end;

    line_start(buffer, error->line, &start);
    GtkSourceMark *mark = gtk_source_buffer_create_source_mark(doc->buffer, NULL,
        error->is_warning ? MARK_WARNING : MARK_ERROR, &start);
    g_object_set_data_full(G_OBJECT(mark), DATA_MESSAGE, g_strdup(error->message), g_free);

    // Underline the code on that line, skipping the indentation
    while (!gtk_text_iter_ends_line(&start) && g_unichar_isspace(gtk_text_iter_get_char(&start))) {
        gtk_text_iter_forward_char(&start);
    }
    end = start;
    if (!gtk_text_iter_ends_line(&end)) gtk_text_iter_forward_to_line_end(&end);
    gtk_text_buffer_apply_tag_by_name(buffer, error->is_warning ? TAG_WARNING : TAG_ERROR, &start, &end);
}

void
photon_document_show_errors(PhotonDocument *doc, GPtrArray *errors) {
    photon_document_clear_errors(doc);
    if (doc->path == NULL || errors == NULL) return;

    gchar *mine = g_canonicalize_filename(doc->path, NULL);
    for (guint i = 0; i < errors->len; i++) {
        const PhotonError *error = g_ptr_array_index(errors, i);
        if (g_strcmp0(error->path, mine) == 0) {
            mark_line(doc, error);
            doc->has_errors = TRUE;
        }
    }
    g_free(mine);
}

void
photon_document_goto(PhotonDocument *doc, gint line, gint column) {
    GtkTextBuffer *buffer = GTK_TEXT_BUFFER(doc->buffer);
    GtkTextIter iter;

    line_start(buffer, line, &iter);
    if (column > 1) {
        gint chars = gtk_text_iter_get_chars_in_line(&iter);
        gtk_text_iter_set_line_offset(&iter, MIN(column - 1, MAX(chars - 1, 0)));
    }
    gtk_text_buffer_place_cursor(buffer, &iter);
    gtk_text_view_scroll_to_iter(GTK_TEXT_VIEW(doc->view), &iter, 0.2, FALSE, 0, 0);
    gtk_widget_grab_focus(doc->view);
}

// ---------------------------------------------------------------------
// Getters
// ---------------------------------------------------------------------
const gchar *
photon_document_get_path(PhotonDocument *doc) {
    return doc->path;
}

const PhotonLanguage *
photon_document_get_language(PhotonDocument *doc) {
    return doc->language;
}

gboolean
photon_document_is_dirty(PhotonDocument *doc) {
    return gtk_text_buffer_get_modified(GTK_TEXT_BUFFER(doc->buffer));
}

GtkSourceBuffer *
photon_document_get_buffer(PhotonDocument *doc) {
    return doc->buffer;
}

GtkSourceView *
photon_document_get_view(PhotonDocument *doc) {
    return GTK_SOURCE_VIEW(doc->view);
}

GtkWidget *
photon_document_get_widget(PhotonDocument *doc) {
    return doc->scroll;
}
