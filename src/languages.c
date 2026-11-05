/**
 * @author: Ahmad Odeh
 * @date: 5th Nov, 2026
 *
**/
/*
 * languages: file -> GtkSourceView language lookup. See languages.h.
 */
#include "languages.h"

void
photon_languages_init(const gchar *data_dir) {
    if (data_dir == NULL) return;

    GtkSourceLanguageManager *manager = gtk_source_language_manager_get_default();
    const gchar * const *old_paths = gtk_source_language_manager_get_search_path(manager);

    // Ours goes first so the repo's lunar.lang wins over an older installed copy
    GPtrArray *paths = g_ptr_array_new_with_free_func(g_free);
    g_ptr_array_add(paths, g_build_filename(data_dir, "lang", NULL));
    for (int i = 0; old_paths != NULL && old_paths[i] != NULL; i++) {
        g_ptr_array_add(paths, g_strdup(old_paths[i]));
    }
    g_ptr_array_add(paths, NULL);

    gtk_source_language_manager_set_search_path(manager, (gchar **)paths->pdata);
    g_ptr_array_free(paths, TRUE);
}

GtkSourceLanguage *
photon_languages_for_file(const gchar *path) {
    if (path == NULL) return NULL;
    GtkSourceLanguageManager *manager = gtk_source_language_manager_get_default();
    return gtk_source_language_manager_guess_language(manager, path, NULL);
}

GtkSourceLanguage *
photon_languages_get(const gchar *id) {
    GtkSourceLanguageManager *manager = gtk_source_language_manager_get_default();
    return gtk_source_language_manager_get_language(manager, id);
}
