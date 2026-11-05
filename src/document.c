/**
 * @author: Ahmad Odeh
 * @date: 5th Nov, 2026
 *
**/
/*
 * document: one open file (buffer + view + path). See document.h.
 */
#include "document.h"
#include "languages.h"
#include "theme.h"

struct PhotonDocument {
    GtkSourceBuffer *buffer;
    GtkWidget       *view;
    GtkWidget       *scroll;   // root widget, holds the view
    gchar           *path;     // NULL until loaded or saved
};

PhotonDocument *
photon_document_new(void) {
    PhotonDocument *doc = g_new0(PhotonDocument, 1);

    // No language yet: it's picked per file in set_path()
    doc->buffer = gtk_source_buffer_new(NULL);
    photon_theme_style_buffer(doc->buffer);

    doc->view = gtk_source_view_new_with_buffer(doc->buffer);
    gtk_source_view_set_show_line_numbers(GTK_SOURCE_VIEW(doc->view), TRUE);

    doc->scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(doc->scroll),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_container_add(GTK_CONTAINER(doc->scroll), doc->view);
    g_object_ref_sink(doc->scroll); // we own it until free, whoever packs it

    return doc;
}

void
photon_document_free(PhotonDocument *doc) {
    if (doc == NULL) return;
    g_object_unref(doc->scroll);
    g_object_unref(doc->buffer);
    g_free(doc->path);
    g_free(doc);
}

void
photon_document_set_path(PhotonDocument *doc, const gchar *path) {
    // Duplicate BEFORE freeing: `path` may be doc->path itself
    gchar *copy = g_strdup(path);
    g_free(doc->path);
    doc->path = copy;

    // "Save As x.lunar" switches highlighting; NULL language = plain text
    gtk_source_buffer_set_language(doc->buffer, photon_languages_for_file(doc->path));
}

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

const gchar *
photon_document_get_path(PhotonDocument *doc) {
    return doc->path;
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
