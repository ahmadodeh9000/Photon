/**
 * @author: Ahmad Odeh
 * @date: 5th Nov, 2026
 *
**/
/*
 * errors: turn a program's stderr into a list of PhotonError (file, line,
 * message). One parser per language; languages.c says which one to use.
 * Parsers are pure functions: text in, list out, no GTK.
 */
#ifndef PHOTON_ERRORS_H
#define PHOTON_ERRORS_H

#include <glib.h>

typedef struct PhotonError {
    gchar   *path;        // absolute, canonical
    gint     line;        // 1-based
    gint     column;      // 1-based, 0 = unknown
    gchar   *message;
    gboolean is_warning;
} PhotonError;

// stderr text, the file that was run, the folder it ran in.
// Returns a GPtrArray of PhotonError* that frees its elements.
typedef GPtrArray *(*PhotonErrorParser)(const gchar *output, const gchar *source_path, const gchar *base_dir);

GPtrArray *photon_errors_parse_lunar(const gchar *output, const gchar *source_path, const gchar *base_dir);
GPtrArray *photon_errors_parse_c(const gchar *output, const gchar *source_path, const gchar *base_dir);
GPtrArray *photon_errors_parse_python(const gchar *output, const gchar *source_path, const gchar *base_dir);

// First entry that is an error (not a warning), or NULL.
const PhotonError *photon_errors_first(GPtrArray *errors);

void photon_error_free(PhotonError *error);

#endif
