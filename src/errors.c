/**
 * @author: Ahmad Odeh
 * @date: 5th Nov, 2026
 *
**/
/*
 * errors: stderr parsers for Lunar, C (gcc + clang) and Python. See errors.h.
 */
#include "errors.h"

#include <stdlib.h>

void
photon_error_free(PhotonError *error) {
    if (error == NULL) return;
    g_free(error->path);
    g_free(error->message);
    g_free(error);
}

static GPtrArray *
new_list(void) {
    return g_ptr_array_new_with_free_func((GDestroyNotify)photon_error_free);
}

static void
add_error(GPtrArray *list, const gchar *path, const gchar *base_dir,
          gint line, gint column, const gchar *message, gboolean is_warning) {
    PhotonError *error = g_new0(PhotonError, 1);
    error->path = g_canonicalize_filename(path, base_dir);
    error->line = line;
    error->column = column;
    error->message = g_strdup(message);
    error->is_warning = is_warning;
    g_ptr_array_add(list, error);
}

// Integer from a regex group; 0 if the group didn't match
static gint
group_int(GMatchInfo *match, gint group) {
    gchar *text = g_match_info_fetch(match, group);
    gint value = (text != NULL && *text != '\0') ? atoi(text) : 0;
    g_free(text);
    return value;
}

const PhotonError *
photon_errors_first(GPtrArray *errors) {
    for (guint i = 0; errors != NULL && i < errors->len; i++) {
        const PhotonError *error = g_ptr_array_index(errors, i);
        if (!error->is_warning) return error;
    }
    return NULL;
}

// ---------------------------------------------------------------------
// Lunar
//   compile (exit 110):  [line 3] ERROR at ';': Expected expression.
//                        [lib/x.lunar line 3] ERROR: Unterminated string.
//   runtime (exit 100):  Undefined variable 'x'.
//                        [lib/x.lunar line 3] in helper()     <- innermost
//                        [line 4] in script
// Paths in brackets are relative to the main file's folder.
// ---------------------------------------------------------------------
GPtrArray *
photon_errors_parse_lunar(const gchar *output, const gchar *source_path, const gchar *base_dir) {
    GPtrArray *list = new_list();
    GRegex *compile_re = g_regex_new("^\\[(?:(.+) )?line (\\d+)\\] ERROR(?: at '(.*)')?: (.*)$", 0, 0, NULL);
    GRegex *trace_re = g_regex_new("^\\[(?:(.+) )?line (\\d+)\\] in ", 0, 0, NULL);
    gchar **lines = g_strsplit(output, "\n", -1);
    gchar *pending = NULL;   // runtime message waiting for its first trace line

    for (gint i = 0; lines[i] != NULL; i++) {
        const gchar *line = g_strchomp(lines[i]);
        GMatchInfo *match = NULL;

        if (g_regex_match(compile_re, line, 0, &match)) {
            gchar *file = g_match_info_fetch(match, 1);
            gchar *token = g_match_info_fetch(match, 3);
            gchar *text = g_match_info_fetch(match, 4);
            gchar *message = (token != NULL && *token != '\0')
                ? g_strdup_printf("%s (at '%s')", text, token) : g_strdup(text);
            add_error(list, *file ? file : source_path, base_dir, group_int(match, 2), 0, message, FALSE);
            g_free(file); g_free(token); g_free(text); g_free(message);
        } else if (g_regex_match(trace_re, line, 0, &match)) {
            if (pending != NULL) {
                gchar *file = g_match_info_fetch(match, 1);
                add_error(list, *file ? file : source_path, base_dir, group_int(match, 2), 0, pending, FALSE);
                g_free(file);
                g_clear_pointer(&pending, g_free);
            }
        } else if (*line != '\0' && !g_str_has_prefix(line, "...")) {
            g_free(pending);
            pending = g_strdup(line);
        }
        g_match_info_free(match);
    }

    g_free(pending);
    g_strfreev(lines);
    g_regex_unref(compile_re);
    g_regex_unref(trace_re);
    return list;
}

// ---------------------------------------------------------------------
// C, gcc and clang:   file.c:12:5: error: expected ';' ...
//                     file.c:12: warning: ...          (no column)
// Notes are skipped; linker errors have no location and are skipped too.
// ---------------------------------------------------------------------
GPtrArray *
photon_errors_parse_c(const gchar *output, const gchar *source_path G_GNUC_UNUSED, const gchar *base_dir) {
    GPtrArray *list = new_list();
    GRegex *re = g_regex_new("^(.+?):(\\d+):(?:(\\d+):)? (fatal error|error|warning): (.*)$",
                             G_REGEX_MULTILINE, 0, NULL);
    GMatchInfo *match = NULL;

    g_regex_match(re, output, 0, &match);
    while (g_match_info_matches(match)) {
        gchar *file = g_match_info_fetch(match, 1);
        gchar *kind = g_match_info_fetch(match, 4);
        gchar *text = g_match_info_fetch(match, 5);
        add_error(list, file, base_dir, group_int(match, 2), group_int(match, 3),
                  text, g_strcmp0(kind, "warning") == 0);
        g_free(file); g_free(kind); g_free(text);
        g_match_info_next(match, NULL);
    }

    g_match_info_free(match);
    g_regex_unref(re);
    return list;
}

// ---------------------------------------------------------------------
// Python:   Traceback (most recent call last):
//             File "/path/x.py", line 5, in <module>
//             File "/path/x.py", line 2, in f        <- last frame = where it broke
//           ZeroDivisionError: division by zero      <- last unindented line
// SyntaxError has the same shape with a single File line.
// ---------------------------------------------------------------------
GPtrArray *
photon_errors_parse_python(const gchar *output, const gchar *source_path G_GNUC_UNUSED, const gchar *base_dir) {
    GPtrArray *list = new_list();
    GRegex *frame_re = g_regex_new("^\\s*File \"(.+)\", line (\\d+)", 0, 0, NULL);
    gchar **lines = g_strsplit(output, "\n", -1);
    gchar *frame_path = NULL;
    gint frame_line = 0;
    const gchar *message = NULL;

    for (gint i = 0; lines[i] != NULL; i++) {
        const gchar *line = g_strchomp(lines[i]);
        GMatchInfo *match = NULL;

        if (g_regex_match(frame_re, line, 0, &match)) {
            gchar *file = g_match_info_fetch(match, 1);
            gchar *full = g_canonicalize_filename(file, base_dir);
            // Skip "<frozen ...>" and friends: keep the last frame we can open
            if (g_file_test(full, G_FILE_TEST_IS_REGULAR)) {
                g_free(frame_path);
                frame_path = g_strdup(full);
                frame_line = group_int(match, 2);
            }
            g_free(full);
            g_free(file);
        } else if (*line != '\0' && !g_ascii_isspace(*line) && !g_str_has_prefix(line, "Traceback")) {
            message = line;
        }
        g_match_info_free(match);
    }

    if (frame_path != NULL) {
        add_error(list, frame_path, base_dir, frame_line, 0, message ? message : "Error", FALSE);
    }

    g_free(frame_path);
    g_strfreev(lines);
    g_regex_unref(frame_re);
    return list;
}
