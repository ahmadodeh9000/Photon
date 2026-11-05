/**
 * @author: Ahmad Odeh
 * @date: 5th Nov, 2026
 *
**/
/*
 * editing: typing behaviors on a document's view that depend on its
 * language: auto-closing pairs, smart Enter (indent after "{", split "{}"
 * into a block), dedent on a closing bracket, comment toggling, and the
 * line commands (duplicate, move up/down).
 */
#ifndef PHOTON_EDITING_H
#define PHOTON_EDITING_H

#include "document.h"

// Hook the key handling into the document's view. document.c calls this.
void photon_editing_attach(PhotonDocument *doc);

// Comment or uncomment the selected lines (or the cursor's line) with the
// language's marker ("//" or "#"). Does nothing for unknown languages.
void photon_editing_toggle_comment(PhotonDocument *doc);

// Duplicate the selected lines (or the cursor's line) below themselves.
void photon_editing_duplicate_lines(PhotonDocument *doc);

// Move the selected lines (or the cursor's line) up or down one line.
void photon_editing_move_lines(PhotonDocument *doc, gboolean down);

#endif
