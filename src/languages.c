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

// Completion words. Lunar's come from lang/lunar.lang and the built-in
// module tables (string, fs, os) in Lunar's README.
static const gchar *const LUNAR_WORDS[] = {
    // keywords + literals
    "let", "fn", "struct", "if", "else", "while", "for", "in", "break", "continue",
    "ret", "try", "catch", "throw", "print", "import", "true", "false", "nil", "self", "super",
    // built-in functions
    "clock", "sqrt", "abs", "floor", "ceil", "sin", "cos", "tan", "atan2", "pow", "min", "max",
    "str", "len", "hex", "random", "push", "pop", "has", "keys", "remove", "clib", "cbind",
    // import("string")
    "split", "join", "find", "contains", "starts_with", "ends_with", "replace", "slice",
    "upper", "lower", "trim", "repeat", "chars", "char_count", "to_number", "byte", "from_byte",
    // import("fs")
    "read", "write", "append", "exists", "is_dir", "list", "mkdir", "lines",
    // import("os")
    "args", "env", "exit", "run", "time", "sleep", "platform", "cwd", "input",
    // SDL
    "sdl_init", "sdl_quit", "sdl_poll", "sdl_clear", "sdl_present", "sdl_delay", "sdl_fill_rect",
    "sdl_draw_tri", "sdl_draw_tri_tex", "sdl_draw_tris", "sdl_load_texture", "sdl_draw_texture",
    "sdl_texture_width", "sdl_texture_height", "sdl_key_down", "sdl_set_relative_mouse",
    "sdl_mouse_dx", "sdl_mouse_dy", "sdl_mouse_down", "sdl_time", "sdl_set_vsync", "sdl_set_blend",
    NULL
};

static const gchar *const C_WORDS[] = {
    "auto", "break", "case", "char", "const", "continue", "default", "do", "double", "else",
    "enum", "extern", "float", "for", "goto", "if", "inline", "int", "long", "register",
    "restrict", "return", "short", "signed", "sizeof", "static", "struct", "switch", "typedef",
    "union", "unsigned", "void", "volatile", "while", "bool", "true", "false", "NULL",
    "size_t", "uint8_t", "uint16_t", "uint32_t", "uint64_t", "int8_t", "int16_t", "int32_t", "int64_t",
    "include", "define", "ifdef", "ifndef", "endif", "pragma",
    "printf", "fprintf", "snprintf", "sprintf", "scanf", "sscanf", "puts", "putchar", "getchar",
    "fgets", "fopen", "fclose", "fread", "fwrite", "fflush", "stdin", "stdout", "stderr",
    "malloc", "calloc", "realloc", "free", "exit", "atoi", "strtol", "strtod", "qsort", "abs",
    "strlen", "strcmp", "strncmp", "strcpy", "strncpy", "strcat", "strchr", "strstr", "memcpy",
    "memmove", "memset", "memcmp", "EXIT_SUCCESS", "EXIT_FAILURE",
    NULL
};

static const gchar *const PYTHON_WORDS[] = {
    "False", "None", "True", "and", "as", "assert", "async", "await", "break", "class",
    "continue", "def", "del", "elif", "else", "except", "finally", "for", "from", "global",
    "if", "import", "in", "is", "lambda", "nonlocal", "not", "or", "pass", "raise", "return",
    "try", "while", "with", "yield", "match", "case", "self",
    "print", "input", "len", "range", "enumerate", "zip", "map", "filter", "sorted", "reversed",
    "sum", "min", "max", "abs", "round", "int", "float", "str", "bool", "list", "dict", "set",
    "tuple", "open", "isinstance", "type", "super", "repr", "format", "any", "all", "iter",
    "next", "hasattr", "getattr", "setattr", "Exception", "ValueError", "TypeError",
    "KeyError", "IndexError", "__name__", "__main__", "__init__",
    NULL
};

static const PhotonLanguage LANG_LUNAR = {
    .id = "lunar", .name = "Lunar",
    .exts = { ".lunar", ".lun", NULL },
    .gsv_lang = "lunar",
    .run_cmd = "{lunar} {file}",
    .comment = "//", .indent = 4,
    .pairs = "()[]{}\"\"",
    .indent_after = "{[(",
    .keywords = LUNAR_WORDS,
    .error_parser = photon_errors_parse_lunar,
};

static const PhotonLanguage LANG_C = {
    .id = "c", .name = "C",
    .exts = { ".c", ".h", NULL },
    .gsv_lang = "c",
    .run_cmd = "{cc} -Wall -g {file} -o {exe} && {exe}",
    .comment = "//", .indent = 4,
    .pairs = "()[]{}\"\"''",
    .indent_after = "{[(",
    .keywords = C_WORDS,
    .error_parser = photon_errors_parse_c,
};

static const PhotonLanguage LANG_PYTHON = {
    .id = "python", .name = "Python",
    .exts = { ".py", ".pyw", NULL },
    .gsv_lang = "python3",
    .run_cmd = "{python} {file}",
    .comment = "#", .indent = 4,
    .pairs = "()[]{}\"\"''",
    .indent_after = ":{[(",
    .keywords = PYTHON_WORDS,
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
