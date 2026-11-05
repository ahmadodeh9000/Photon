/**
 * @author: Ahmad Odeh
 * @date: 5th Nov, 2026
 *
**/
/*
 * panel: the bottom panel, a notebook of tabs. Owns the "Terminal" tab (a
 * VteTerminal running the user's shell, restarted after it exits); other
 * modules add their own tabs (runner.c adds "Run").
 */
#ifndef PHOTON_PANEL_H
#define PHOTON_PANEL_H

#include <gtk/gtk.h>

typedef struct PhotonPanel PhotonPanel;

PhotonPanel *photon_panel_new(const gchar *working_dir);
void         photon_panel_free(PhotonPanel *panel);

GtkWidget *photon_panel_get_widget(PhotonPanel *panel);

// Add a tab. The panel shows it; the caller keeps its own widget pointer.
void photon_panel_add_tab(PhotonPanel *panel, GtkWidget *content, const gchar *title);

// Show the panel on the tab holding `content`.
void photon_panel_show_tab(PhotonPanel *panel, GtkWidget *content);

// Terminal tab showing? Hide the panel. Otherwise show it on the terminal
// tab and focus it, starting a fresh shell if the old one exited.
void photon_panel_toggle_terminal(PhotonPanel *panel);

#endif
