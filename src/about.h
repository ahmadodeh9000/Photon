/**
 * @author: Ahmad Odeh
 * @date: 5th Nov, 2026
 *
**/
/*
 * about: the "About Photon" dialog, laid out like a Windows 95 About box
 * (title strip, icon, sunken info well, OK button). The theme's CSS does
 * the styling.
 */
#ifndef PHOTON_ABOUT_H
#define PHOTON_ABOUT_H

#include <gtk/gtk.h>

void photon_about_show(GtkWindow *parent);

// Photon's logo at `size` px (newly referenced), from our own icons/ folder,
// so an older icon installed in a system icon theme can't take its place.
// Falls back to the icon theme; NULL if neither has it.
GdkPixbuf *photon_about_logo(gint size);

#endif
