/**
 * @author: Ahmad Odeh
 * @date: 5th Nov, 2026
 *
**/
/*
 * statusbar: the strip at the bottom of the window:
 *   [ message (errors in red) ...  Ln 3, Col 7 | Lunar ▾ | Exit 0 · 0.42 s ]
 * Clicking the language opens a menu to change it.
 */
#ifndef PHOTON_STATUSBAR_H
#define PHOTON_STATUSBAR_H

#include <gtk/gtk.h>

#include "languages.h"

typedef struct PhotonStatusbar PhotonStatusbar;

// Called with the picked language, or NULL for "Plain Text".
typedef void (*PhotonStatusbarLanguageFunc)(const PhotonLanguage *language, gpointer user_data);

PhotonStatusbar *photon_statusbar_new(PhotonStatusbarLanguageFunc on_language, gpointer user_data);
void             photon_statusbar_free(PhotonStatusbar *bar);

GtkWidget *photon_statusbar_get_widget(PhotonStatusbar *bar);

// Left side. is_error shows it in the error style. NULL clears it.
void photon_statusbar_set_message(PhotonStatusbar *bar, const gchar *text, gboolean is_error);

// Cursor position, 1-based.
void photon_statusbar_set_position(PhotonStatusbar *bar, gint line, gint column);

// Name shown on the language button ("Lunar", "Plain Text").
void photon_statusbar_set_language(PhotonStatusbar *bar, const gchar *name);

// Right side, e.g. "Running…" or "Exit 0 · 0.42 s".
void photon_statusbar_set_run_status(PhotonStatusbar *bar, const gchar *text);

#endif
