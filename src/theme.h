/**
 * @author: Ahmad Odeh
 * @date: 5th Nov, 2026
 *
**/
/*
 * theme: how Photon looks. Owns the GTK CSS for the window chrome, the
 * GtkSourceView style scheme used for code, and the code font size (zoom).
 * The app owns one PhotonTheme and styles every new document with it.
 */
#ifndef PHOTON_THEME_H
#define PHOTON_THEME_H

#include <gtksourceview/gtksource.h>

#define PHOTON_FONT_SIZE_DEFAULT 11
#define PHOTON_FONT_SIZE_MIN      6
#define PHOTON_FONT_SIZE_MAX      40

typedef struct PhotonTheme PhotonTheme;

// Makes <data_dir>/styles and <data_dir>/icons visible, then applies the
// CSS. Call right after gtk_init(). data_dir may be NULL.
PhotonTheme *photon_theme_new(const gchar *data_dir);
void         photon_theme_free(PhotonTheme *theme);

// The style scheme for code, or NULL if it couldn't be found.
GtkSourceStyleScheme *photon_theme_get_scheme(PhotonTheme *theme);

// Apply the style scheme to a buffer.
void photon_theme_style_buffer(PhotonTheme *theme, GtkSourceBuffer *buffer);

// Code font size in points (clamped). Applies to every editor at once.
void photon_theme_set_font_size(PhotonTheme *theme, gint points);
gint photon_theme_get_font_size(PhotonTheme *theme);

#endif
