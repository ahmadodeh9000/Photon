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

// Move the cursor to line/column (1-based, column 0 = line start) and scroll there.
void photon_document_goto(PhotonDocument *doc, gint line, gint column);

#endif
