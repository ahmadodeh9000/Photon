/**
 * @author: Ahmad Odeh
 * @date: 5th Nov, 2026
 *
**/
/*
 * sidebar: the file tree on the left. Lists a root folder lazily (a folder
 * is read when first expanded) and reports activated files via a callback.
 */
#ifndef PHOTON_SIDEBAR_H
#define PHOTON_SIDEBAR_H

#include <gtk/gtk.h>

typedef struct PhotonSidebar PhotonSidebar;

// Called when the user double-clicks / presses Enter on a file.
typedef void (*PhotonSidebarOpenFunc)(const gchar *path, gpointer user_data);

PhotonSidebar *photon_sidebar_new(const gchar *root_dir, PhotonSidebarOpenFunc on_open, gpointer user_data);
void           photon_sidebar_free(PhotonSidebar *sidebar);

GtkWidget *photon_sidebar_get_widget(PhotonSidebar *sidebar);
void       photon_sidebar_toggle(PhotonSidebar *sidebar);

// Re-read the folder (after a save / new file), keeping open folders open.
void       photon_sidebar_refresh(PhotonSidebar *sidebar);

#endif
