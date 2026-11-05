/**
 * @author: Ahmad Odeh
 * @date: 5th Nov, 2026
 *
**/
/*
 * languages: the language registry. Everything Photon knows about a
 * language (extensions, highlighting, how to run it, comment marker,
 * indent, auto-close pairs, error parser) is ONE entry in languages.c.
 * Also makes Photon's own lang/ folder visible to GtkSourceView.
 */
#ifndef PHOTON_LANGUAGES_H
#define PHOTON_LANGUAGES_H

#include <gtksourceview/gtksource.h>

#include "errors.h"

#define PHOTON_MAX_EXTS 8

typedef struct PhotonLanguage {
    const gchar      *id;
    const gchar      *name;
    const gchar      *exts[PHOTON_MAX_EXTS];  // NULL-terminated, with the dot
    const gchar      *gsv_lang;     // GtkSourceView language id
    // Shell command run in the file's folder. Placeholders (shell-quoted):
    //   {file} {dir} {name} (file name without extension) {exe} (temp binary)
    //   {lunar} {python} {cc}  (tools, see runner.c)
    const gchar      *run_cmd;      // NULL = can't be run
    const gchar      *comment;      // line comment marker
    guint             indent;       // spaces per indent level
    const gchar      *pairs;        // auto-close pairs, 2 chars each: "()[]"
    PhotonErrorParser error_parser; // NULL = no error markers
} PhotonLanguage;

// Put <data_dir>/lang first in GtkSourceView's search path. Call once at
// startup, before any language is looked up. data_dir may be NULL.
void photon_languages_init(const gchar *data_dir);

// Registry entry for a file name (by extension), or NULL.
const PhotonLanguage *photon_languages_for_file(const gchar *path);

// Registry entry by id ("lunar", "c", "python"), or NULL.
const PhotonLanguage *photon_languages_find(const gchar *id);

// All registry entries, NULL-terminated (for menus).
const PhotonLanguage *const *photon_languages_all(void);

// GtkSourceView language for a file: the registry's if it has one, else
// GtkSourceView's own guess (Makefile, Markdown, ...). NULL = plain text.
GtkSourceLanguage *photon_languages_highlighting_for_file(const gchar *path);

// GtkSourceView language by id ("lunar", "c", "python3"), or NULL.
GtkSourceLanguage *photon_languages_get(const gchar *gsv_id);

#endif
