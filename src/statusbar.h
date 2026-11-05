/**
 * @author: Ahmad Odeh
 * @date: 5th Nov, 2026
 *
**/
/*
 * statusbar: the strip at the bottom of the window. A message on the left
 * (errors in red) and the run status on the right.
 */
#ifndef PHOTON_STATUSBAR_H
#define PHOTON_STATUSBAR_H

#include <gtk/gtk.h>

typedef struct PhotonStatusbar PhotonStatusbar;

PhotonStatusbar *photon_statusbar_new(void);
void             photon_statusbar_free(PhotonStatusbar *bar);

GtkWidget *photon_statusbar_get_widget(PhotonStatusbar *bar);

// Left side. is_error shows it in the error style. NULL clears it.
void photon_statusbar_set_message(PhotonStatusbar *bar, const gchar *text, gboolean is_error);

// Right side, e.g. "Running…" or "Exit 0 · 0.42 s".
void photon_statusbar_set_run_status(PhotonStatusbar *bar, const gchar *text);

#endif
