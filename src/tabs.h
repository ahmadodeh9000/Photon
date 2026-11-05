/**
 * @author: Ahmad Odeh
 * @date: 5th Nov, 2026
 *
**/
/*
 * tabs: the notebook of open documents. Owns the documents in it, draws
 * each tab (name, "*" when modified, close button) and tells the app when
 * the current tab changes, a tab asks to close, or a document changes.
 * It never asks the user anything: the app decides what closing means.
 */
#ifndef PHOTON_TABS_H
#define PHOTON_TABS_H

#include <gtk/gtk.h>

#include "document.h"

typedef struct PhotonTabs PhotonTabs;

typedef struct {
    void (*close_requested)(PhotonDocument *doc, gpointer user_data);  // tab's x clicked
    void (*switched)(PhotonDocument *doc, gpointer user_data);         // current tab changed
    PhotonDocumentListener doc_changed;                                // any document changed
    gpointer user_data;
} PhotonTabsCallbacks;

PhotonTabs *photon_tabs_new(const PhotonTabsCallbacks *callbacks);
void        photon_tabs_free(PhotonTabs *tabs);   // frees every document too

GtkWidget *photon_tabs_get_widget(PhotonTabs *tabs);

// Takes ownership of `doc`, adds a tab for it and selects it.
void photon_tabs_add(PhotonTabs *tabs, PhotonDocument *doc);

// Removes the tab and frees the document. No questions asked.
void photon_tabs_close(PhotonTabs *tabs, PhotonDocument *doc);

void            photon_tabs_select(PhotonTabs *tabs, PhotonDocument *doc);
PhotonDocument *photon_tabs_current(PhotonTabs *tabs);              // NULL if no tabs
PhotonDocument *photon_tabs_find(PhotonTabs *tabs, const gchar *path); // by canonical path
guint           photon_tabs_count(PhotonTabs *tabs);
gint            photon_tabs_current_index(PhotonTabs *tabs);

// Documents in tab order. Free the list (not the documents) with g_list_free.
GList *photon_tabs_list(PhotonTabs *tabs);

#endif
