/**
 * @author: Ahmad Odeh
 * @date: 5th Nov, 2026
 *
**/
/*
 * languages: which GtkSourceView language a file gets. Also makes Photon's
 * own lang/ folder visible to GtkSourceView, so nothing has to be installed
 * for Lunar highlighting to work.
 */
#ifndef PHOTON_LANGUAGES_H
#define PHOTON_LANGUAGES_H

#include <gtksourceview/gtksource.h>

// Put <data_dir>/lang first in GtkSourceView's search path. Call once at
// startup, before any language is looked up. data_dir may be NULL.
void photon_languages_init(const gchar *data_dir);

// Language for a file name (by extension), or NULL for plain text.
GtkSourceLanguage *photon_languages_for_file(const gchar *path);

// Language by GtkSourceView id ("lunar", "c", "python3"), or NULL.
GtkSourceLanguage *photon_languages_get(const gchar *id);

#endif
