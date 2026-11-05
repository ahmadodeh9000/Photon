/**
 * @author: Ahmad Odeh
 * @date: 5th Nov, 2026
 *
**/
/*
 * editing: typing behaviors on a document's view that depend on its
 * language: auto-closing pairs and toggling line comments.
 */
#ifndef PHOTON_EDITING_H
#define PHOTON_EDITING_H

#include "document.h"

// Hook the key handling into the document's view. document.c calls this.
void photon_editing_attach(PhotonDocument *doc);

// Comment or uncomment the selected lines (or the cursor's line) with the
// language's marker ("//" or "#"). Does nothing for unknown languages.
void photon_editing_toggle_comment(PhotonDocument *doc);

#endif
