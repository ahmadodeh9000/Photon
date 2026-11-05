/**
 * @author: Ahmad Odeh
 * @date: 5th Nov, 2026
 *
**/
/*
 * menus: the menu bar and every keyboard shortcut. Each item is one line
 * in a build_*_menu() function; its callback calls into app.c / the
 * modules. Shortcuts are always "<Primary>..." (Cmd on macOS, Ctrl on Linux).
 */
#ifndef PHOTON_MENUS_H
#define PHOTON_MENUS_H

#include "app.h"

// Builds the menu bar and attaches its shortcuts to app->window.
GtkWidget *photon_menus_build(PhotonApp *app);

#endif
