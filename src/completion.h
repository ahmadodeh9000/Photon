/**
 * @author: Ahmad Odeh
 * @date: 5th Nov, 2026
 *
**/
/*
 * completion: autocomplete in every document. Two providers per view:
 *   "Words"     words from all open buffers (GtkSourceCompletionWords)
 *   <language>  the language's keywords + built-ins from the registry
 * The keyword provider is a CompletionWords fed from a hidden buffer that
 * holds the word list, so no custom GObject is needed.
 */
#ifndef PHOTON_COMPLETION_H
#define PHOTON_COMPLETION_H

#include "document.h"

typedef struct PhotonCompletion PhotonCompletion;

PhotonCompletion *photon_completion_new(void);
void              photon_completion_free(PhotonCompletion *completion);

// Hook a new document up; call again when its language changes.
void photon_completion_attach(PhotonCompletion *completion, PhotonDocument *doc);

// Before the document is freed.
void photon_completion_detach(PhotonCompletion *completion, PhotonDocument *doc);

#endif
