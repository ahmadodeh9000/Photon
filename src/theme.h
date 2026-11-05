/**
 * @author: Ahmad Odeh
 * @date: 5th Nov, 2026
 *
**/
/*
 * theme: how Photon looks. Owns the GTK CSS for the window chrome and the
 * GtkSourceView style scheme used for code.
 */
#ifndef PHOTON_THEME_H
#define PHOTON_THEME_H

#include <gtksourceview/gtksource.h>

// Make <data_dir>/styles and <data_dir>/icons visible, then apply the CSS.
// Call once, right after gtk_init(). data_dir may be NULL.
void photon_theme_init(const gchar *data_dir);

// The style scheme for code, or NULL if it couldn't be found.
GtkSourceStyleScheme *photon_theme_get_scheme(void);

// Apply the current style scheme to a buffer.
void photon_theme_style_buffer(GtkSourceBuffer *buffer);

#endif
