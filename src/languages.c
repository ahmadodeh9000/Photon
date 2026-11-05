/**
 * @author: Ahmad Odeh
 * @date: 5th Nov, 2026
 *
**/
/*
 * languages: the language registry. See languages.h.
 *
 * To add a language, add one entry to LANGUAGES below. If GtkSourceView
 * doesn't ship a .lang for it, drop one in lang/ (see lunar.lang).
 */
#include "languages.h"

static const PhotonLanguage LANG_LUNAR = {
    .id = "lunar", .name = "Lunar",
    .exts = { ".lunar", ".lun", NULL },
    .gsv_lang = "lunar",
    .run_cmd = "{lunar} {file}",
    .comment = "//", .indent = 4,
    .pairs = "()[]{}\"\"",
    .error_parser = photon_errors_parse_lunar,
};

static const PhotonLanguage LANG_C = {
    .id = "c", .name = "C",
    .exts = { ".c", ".h", NULL },
    .gsv_lang = "c",
    .run_cmd = "{cc} -Wall -g {file} -o {exe} && {exe}",
    .comment = "//", .indent = 4,
    .pairs = "()[]{}\"\"''",
    .error_parser = photon_errors_parse_c,
};

static const PhotonLanguage LANG_PYTHON = {
    .id = "python", .name = "Python",
    .exts = { ".py", ".pyw", NULL },
    .gsv_lang = "python3",
    .run_cmd = "{python} {file}",
    .comment = "#", .indent = 4,
    .pairs = "()[]{}\"\"''",
    .error_parser = photon_errors_parse_python,
};

static const PhotonLanguage *const LANGUAGES[] = {
    &LANG_LUNAR,
    &LANG_C,
    &LANG_PYTHON,
    NULL
};

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

const PhotonLanguage *const *
photon_languages_all(void) {
    return LANGUAGES;
}

const PhotonLanguage *
photon_languages_find(const gchar *id) {
    for (int i = 0; LANGUAGES[i] != NULL; i++) {
        if (g_strcmp0(LANGUAGES[i]->id, id) == 0) return LANGUAGES[i];
    }
    return NULL;
}

const PhotonLanguage *
photon_languages_for_file(const gchar *path) {
    if (path == NULL) return NULL;

    gchar *lower = g_ascii_strdown(path, -1);
    const PhotonLanguage *found = NULL;
    for (int i = 0; found == NULL && LANGUAGES[i] != NULL; i++) {
        for (int e = 0; LANGUAGES[i]->exts[e] != NULL; e++) {
            if (g_str_has_suffix(lower, LANGUAGES[i]->exts[e])) {
                found = LANGUAGES[i];
                break;
            }
        }
    }
    g_free(lower);
    return found;
}

GtkSourceLanguage *
photon_languages_get(const gchar *gsv_id) {
    return gtk_source_language_manager_get_language(gtk_source_language_manager_get_default(), gsv_id);
}

GtkSourceLanguage *
photon_languages_highlighting_for_file(const gchar *path) {
    if (path == NULL) return NULL;

    const PhotonLanguage *lang = photon_languages_for_file(path);
    if (lang != NULL) return photon_languages_get(lang->gsv_lang);

    return gtk_source_language_manager_guess_language(gtk_source_language_manager_get_default(), path, NULL);
}
