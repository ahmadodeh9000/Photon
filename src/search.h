/**
 * @author: Ahmad Odeh
 * @date: 5th Nov, 2026
 *
**/
/*
 * search: the find / replace bar above the editor (GtkSourceSearchContext,
 * one per buffer, all sharing the same settings) and the go-to-line dialog.
 * Works on whatever document the app says is current.
 */
#ifndef PHOTON_SEARCH_H
#define PHOTON_SEARCH_H

#include <gtk/gtk.h>

#include "document.h"

typedef struct PhotonSearch PhotonSearch;

// Returns the document to search in (the current tab), or NULL.
typedef PhotonDocument *(*PhotonSearchTarget)(gpointer user_data);

PhotonSearch *photon_search_new(PhotonSearchTarget target, gpointer user_data);
void          photon_search_free(PhotonSearch *search);

GtkWidget *photon_search_get_widget(PhotonSearch *search);

// Show the bar (with the replace row if `replace`), filled with the
// selected text, and focus it.
void photon_search_show(PhotonSearch *search, gboolean replace);
void photon_search_hide(PhotonSearch *search);

void photon_search_next(PhotonSearch *search);
void photon_search_previous(PhotonSearch *search);

// The current tab changed: move highlighting over to it.
void photon_search_document_changed(PhotonSearch *search);

// Ask for "line" or "line:column" and jump there.
void photon_search_goto_line(GtkWindow *parent, PhotonDocument *doc);

#endif
