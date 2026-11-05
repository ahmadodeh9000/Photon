/**
 * @author: Ahmad Odeh
 * @date: 5th Nov, 2026
 *
**/
/*
 * theme: how Photon looks. A theme is one entry in theme.c: GTK CSS for
 * the chrome, a GtkSourceView style scheme for code (styles/<id>.xml),
 * terminal colors + font, the error-line tint, and whether the title
 * strip shows. The PhotonTheme also remembers every buffer and terminal
 * it styled, so switching themes restyles them all at once. Font zoom
 * lives here too.
 */
#ifndef PHOTON_THEME_H
#define PHOTON_THEME_H

#include <gtksourceview/gtksource.h>
#include <vte/vte.h>

#define PHOTON_FONT_SIZE_DEFAULT 11
#define PHOTON_FONT_SIZE_MIN      6
#define PHOTON_FONT_SIZE_MAX      40
#define PHOTON_THEME_DEFAULT      "turbo90s"

typedef struct PhotonTheme PhotonTheme;

// Makes <data_dir>/styles and <data_dir>/icons visible, then applies the
// theme `id` (or the default if unknown). Call right after gtk_init().
PhotonTheme *photon_theme_new(const gchar *data_dir, const gchar *id);
void         photon_theme_free(PhotonTheme *theme);

// Theme ids in menu order, NULL-terminated, and their display names.
const gchar *const *photon_theme_ids(void);
const gchar        *photon_theme_name(const gchar *id);

// Switch themes; restyles everything attached. FALSE if `id` is unknown.
gboolean     photon_theme_set(PhotonTheme *theme, const gchar *id);
const gchar *photon_theme_get_id(PhotonTheme *theme);

// Style now and on every later switch (until the object is finalized).
void photon_theme_attach_buffer(PhotonTheme *theme, GtkSourceBuffer *buffer);
void photon_theme_attach_terminal(PhotonTheme *theme, VteTerminal *terminal);

// The style scheme for code, or NULL if it couldn't be found.
GtkSourceStyleScheme *photon_theme_get_scheme(PhotonTheme *theme);

// Per-theme details the app applies itself.
gboolean     photon_theme_has_title_strip(PhotonTheme *theme);
const gchar *photon_theme_error_tint(PhotonTheme *theme);     // CSS color for error lines
const gchar *photon_theme_warning_tint(PhotonTheme *theme);

// Make a scrolled window a sunken "well" (classic, non-overlay scrollbars).
void photon_theme_make_well(GtkWidget *scrolled_window);

// Code font size in points (clamped). Applies to every editor at once.
void photon_theme_set_font_size(PhotonTheme *theme, gint points);
gint photon_theme_get_font_size(PhotonTheme *theme);

#endif
