/**
 * @author: Ahmad Odeh
 * @date: 5th Nov, 2026
 *
**/
/*
 * document: one open file. Owns its GtkSourceBuffer, the GtkSourceView
 * showing it (inside a scrolled window), its path, its language entry, its
 * dirty state and its error markers. Knows nothing about windows, menus or
 * dialogs.
 */
#ifndef PHOTON_DOCUMENT_H
#define PHOTON_DOCUMENT_H

#include <gtksourceview/gtksource.h>

#include "languages.h"

typedef struct PhotonDocument PhotonDocument;

// What changed, for the one listener a document has (tabs.c)
typedef enum {
    PHOTON_DOC_DIRTY    = 1 << 0,   // modified flag flipped
    PHOTON_DOC_PATH     = 1 << 1,   // loaded / saved under a new name
    PHOTON_DOC_LANGUAGE = 1 << 2,   // language entry changed
} PhotonDocChange;

typedef void (*PhotonDocumentListener)(PhotonDocument *doc, PhotonDocChange what, gpointer user_data);

PhotonDocument *photon_document_new(void);
void            photon_document_free(PhotonDocument *doc);

// Read a file into the buffer. Refuses binary / non-UTF-8 files.
gboolean photon_document_load(PhotonDocument *doc, const gchar *path, GError **error);

// Write the buffer to `path` (which becomes the document's path).
gboolean photon_document_save_as(PhotonDocument *doc, const gchar *path, GError **error);

// Write the buffer to the document's current path. Fails if it has none.
gboolean photon_document_save(PhotonDocument *doc, GError **error);

// Point the document at a file that doesn't exist yet (so Save writes there).
// Also picks the language (highlighting, indent, comment marker) from it.
void photon_document_set_path(PhotonDocument *doc, const gchar *path);

// Force a language (new files, the status bar picker). NULL = plain text.
void photon_document_set_language(PhotonDocument *doc, const PhotonLanguage *language);

void photon_document_set_listener(PhotonDocument *doc, PhotonDocumentListener listener, gpointer user_data);

// File name for tabs and the title bar: "main.lunar", or "Untitled.lunar".
gchar *photon_document_get_title(PhotonDocument *doc);

// Untitled, unmodified and empty: safe to replace with an opened file.
gboolean photon_document_is_blank(PhotonDocument *doc);

const gchar          *photon_document_get_path(PhotonDocument *doc);      // NULL if untitled
const PhotonLanguage *photon_document_get_language(PhotonDocument *doc);  // NULL if unknown
gboolean              photon_document_is_dirty(PhotonDocument *doc);
GtkSourceBuffer      *photon_document_get_buffer(PhotonDocument *doc);
GtkSourceView        *photon_document_get_view(PhotonDocument *doc);
GtkWidget            *photon_document_get_widget(PhotonDocument *doc);    // the scrolled window

// Error markers: underline + gutter mark (with the message as tooltip) on
// every PhotonError whose path is this document's. Editing clears them.
void photon_document_show_errors(PhotonDocument *doc, GPtrArray *errors);
void photon_document_clear_errors(PhotonDocument *doc);

// Background of error / warning lines (CSS colors), set by the theme.
void photon_document_set_marker_tint(PhotonDocument *doc, const gchar *error_tint, const gchar *warning_tint);

// Move the cursor to line/column (1-based, column 0 = line start) and scroll there.
void photon_document_goto(PhotonDocument *doc, gint line, gint column);

#endif
