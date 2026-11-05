/**
 * @author: Ahmad Odeh
 * @date: 5th Nov, 2026
 *
**/
/*
 * panel: the bottom panel. Holds the interactive terminal (a VteTerminal
 * running the user's shell) and restarts the shell after it exits.
 */
#ifndef PHOTON_PANEL_H
#define PHOTON_PANEL_H

#include <gtk/gtk.h>

typedef struct PhotonPanel PhotonPanel;

PhotonPanel *photon_panel_new(const gchar *working_dir);
void         photon_panel_free(PhotonPanel *panel);

GtkWidget *photon_panel_get_widget(PhotonPanel *panel);

// Show (and focus) or hide the panel. Starts a fresh shell if the old one exited.
void photon_panel_toggle(PhotonPanel *panel);

#endif
