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

// How error / warning lines look in the editor (CSS colors)
typedef struct {
    const gchar *error_line;     // line background
    const gchar *warning_line;
    const gchar *error_dot;      // gutter mark
    const gchar *warning_dot;
} PhotonMarkerColors;

// Makes <data_dir>/styles and <data_dir>/icons visible, then applies the
// theme `id` (or the default if unknown). Call right after gtk_init().
PhotonTheme *photon_theme_new(const gchar *data_dir, const gchar *id);
void         photon_theme_free(PhotonTheme *theme);

// Theme ids in menu order, NULL-terminated, and their display names.
const gchar *const *photon_theme_ids(void);
const gchar        *photon_theme_name(const gchar *id);

// Switch themes; restyles everything attached. FALSE if `id` is unknown
// or its CSS has an error.
gboolean     photon_theme_set(PhotonTheme *theme, const gchar *id);
const gchar *photon_theme_get_id(PhotonTheme *theme);

// Style now and on every later switch (until the object is finalized).
void photon_theme_attach_buffer(PhotonTheme *theme, GtkSourceBuffer *buffer);
void photon_theme_attach_terminal(PhotonTheme *theme, VteTerminal *terminal);

// The style scheme for code, or NULL if it couldn't be found.
GtkSourceStyleScheme *photon_theme_get_scheme(PhotonTheme *theme);

// Per-theme details the app applies itself.
gboolean     photon_theme_has_title_strip(PhotonTheme *theme);
const PhotonMarkerColors *photon_theme_marker_colors(PhotonTheme *theme);

// Make a scrolled window a sunken "well" (classic, non-overlay scrollbars).
void photon_theme_make_well(GtkWidget *scrolled_window);

// ---- Fonts ----------------------------------------------------------
// The user's choice. A NULL family means "the theme's suggestion", a size
// of 0 the default size. The choice always wins over the theme.
typedef struct {
    gchar *code_family;   // editor + terminals
    gint   code_points;
    gchar *ui_family;     // menus, tabs, dialogs, sidebar, status bar
    gint   ui_points;
} PhotonFonts;

void photon_fonts_clear(PhotonFonts *fonts);                         // free the strings, zero it
void photon_fonts_copy(PhotonFonts *dest, const PhotonFonts *src);    // dest must be clear

void               photon_theme_set_fonts(PhotonTheme *theme, const PhotonFonts *fonts);   // copies
const PhotonFonts *photon_theme_get_fonts(PhotonTheme *theme);

// Zoom: steps on top of the chosen code size (0 = the chosen size).
void photon_theme_set_zoom(PhotonTheme *theme, gint steps);
gint photon_theme_get_zoom(PhotonTheme *theme);

// The code size actually in use: chosen size + zoom, clamped.
gint photon_theme_code_points(PhotonTheme *theme);

#endif
