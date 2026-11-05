/**
 * @author: Ahmad Odeh
 * @date: 5th Nov, 2026
 *
**/
/*
 * settings: Photon's settings.ini (a GKeyFile) in
 * g_get_user_config_dir()/photon/. Every set_* writes the file right away.
 */
#ifndef PHOTON_SETTINGS_H
#define PHOTON_SETTINGS_H

#include <glib.h>

typedef struct PhotonSettings PhotonSettings;

// Missing file = empty settings. read_only: changes stay in memory, the
// file is never written (smoke tests, screenshots).
PhotonSettings *photon_settings_load(gboolean read_only);
void            photon_settings_free(PhotonSettings *settings);

// Newly allocated string, or NULL if not set.
gchar *photon_settings_get_string(PhotonSettings *settings, const gchar *group, const gchar *key);
void   photon_settings_set_string(PhotonSettings *settings, const gchar *group, const gchar *key, const gchar *value);

// NULL-terminated list (free with g_strfreev), or NULL if not set.
gchar **photon_settings_get_strv(PhotonSettings *settings, const gchar *group, const gchar *key);
void    photon_settings_set_strv(PhotonSettings *settings, const gchar *group, const gchar *key, const gchar *const *value);

// `fallback` if not set.
gint photon_settings_get_int(PhotonSettings *settings, const gchar *group, const gchar *key, gint fallback);
void photon_settings_set_int(PhotonSettings *settings, const gchar *group, const gchar *key, gint value);

#endif
