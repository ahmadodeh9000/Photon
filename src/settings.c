/**
 * @author: Ahmad Odeh
 * @date: 5th Nov, 2026
 *
**/
/*
 * settings: settings.ini via GKeyFile. See settings.h.
 */
#include "settings.h"

#include <glib/gstdio.h>

struct PhotonSettings {
    GKeyFile *keys;
    gchar    *path;
};

PhotonSettings *
photon_settings_load(void) {
    PhotonSettings *settings = g_new0(PhotonSettings, 1);
    settings->keys = g_key_file_new();
    settings->path = g_build_filename(g_get_user_config_dir(), "photon", "settings.ini", NULL);

    GError *error = NULL;
    if (!g_key_file_load_from_file(settings->keys, settings->path, G_KEY_FILE_KEEP_COMMENTS, &error)) {
        if (!g_error_matches(error, G_FILE_ERROR, G_FILE_ERROR_NOENT)) {
            g_printerr("Could not read %s: %s\n", settings->path, error->message);
        }
        g_error_free(error);
    }
    return settings;
}

void
photon_settings_free(PhotonSettings *settings) {
    if (settings == NULL) return;
    g_key_file_free(settings->keys);
    g_free(settings->path);
    g_free(settings);
}

static void
save(PhotonSettings *settings) {
    gchar *dir = g_path_get_dirname(settings->path);
    g_mkdir_with_parents(dir, 0700);
    g_free(dir);

    GError *error = NULL;
    if (!g_key_file_save_to_file(settings->keys, settings->path, &error)) {
        g_printerr("Could not write %s: %s\n", settings->path, error->message);
        g_error_free(error);
    }
}

gchar *
photon_settings_get_string(PhotonSettings *settings, const gchar *group, const gchar *key) {
    return g_key_file_get_string(settings->keys, group, key, NULL);
}

void
photon_settings_set_string(PhotonSettings *settings, const gchar *group, const gchar *key, const gchar *value) {
    g_key_file_set_string(settings->keys, group, key, value);
    save(settings);
}
