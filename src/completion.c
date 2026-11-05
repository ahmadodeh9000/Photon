/**
 * @author: Ahmad Odeh
 * @date: 5th Nov, 2026
 *
**/
/*
 * completion: buffer words + language keywords. See completion.h.
 */
#include "completion.h"

#define DATA_WORDS_ON  "photon-words-provider"
#define DATA_KEYWORDS  "photon-keyword-provider"

struct PhotonCompletion {
    GtkSourceCompletionWords *words;      // shared: words from every open buffer
    GHashTable               *keywords;   // language id -> GtkSourceCompletionWords
};

PhotonCompletion *
photon_completion_new(void) {
    PhotonCompletion *completion = g_new0(PhotonCompletion, 1);
    completion->words = gtk_source_completion_words_new("Words", NULL);
    g_object_set(completion->words, "minimum-word-size", 3, "priority", 1, NULL);
    completion->keywords = g_hash_table_new_full(g_str_hash, g_str_equal, NULL, g_object_unref);
    return completion;
}

void
photon_completion_free(PhotonCompletion *completion) {
    if (completion == NULL) return;
    g_object_unref(completion->words);
    g_hash_table_destroy(completion->keywords);
    g_free(completion);
}

// One provider per language, built the first time it's needed
static GtkSourceCompletionWords *
keywords_for(PhotonCompletion *completion, const PhotonLanguage *lang) {
    if (lang == NULL || lang->keywords == NULL) return NULL;

    GtkSourceCompletionWords *provider = g_hash_table_lookup(completion->keywords, lang->id);
    if (provider != NULL) return provider;

    provider = gtk_source_completion_words_new(lang->name, NULL);
    g_object_set(provider, "minimum-word-size", 2, "priority", 2, NULL);

    gchar *text = g_strjoinv("\n", (gchar **)lang->keywords);
    GtkTextBuffer *list = gtk_text_buffer_new(NULL);
    gtk_text_buffer_set_text(list, text, -1);
    gtk_source_completion_words_register(provider, list);
    g_object_set_data_full(G_OBJECT(provider), "photon-word-list", list, g_object_unref);
    g_free(text);

    g_hash_table_insert(completion->keywords, (gpointer)lang->id, provider);
    return provider;
}

void
photon_completion_attach(PhotonCompletion *completion, PhotonDocument *doc) {
    GtkSourceView *view = photon_document_get_view(doc);
    GtkSourceCompletion *source = gtk_source_view_get_completion(view);

    if (g_object_get_data(G_OBJECT(view), DATA_WORDS_ON) == NULL) {
        gtk_source_completion_words_register(completion->words, GTK_TEXT_BUFFER(photon_document_get_buffer(doc)));
        gtk_source_completion_add_provider(source, GTK_SOURCE_COMPLETION_PROVIDER(completion->words), NULL);
        g_object_set_data(G_OBJECT(view), DATA_WORDS_ON, completion->words);
    }

    // Swap the keyword provider if the language changed
    GtkSourceCompletionWords *old = g_object_get_data(G_OBJECT(view), DATA_KEYWORDS);
    GtkSourceCompletionWords *wanted = keywords_for(completion, photon_document_get_language(doc));
    if (old == wanted) return;
    if (old != NULL) gtk_source_completion_remove_provider(source, GTK_SOURCE_COMPLETION_PROVIDER(old), NULL);
    if (wanted != NULL) gtk_source_completion_add_provider(source, GTK_SOURCE_COMPLETION_PROVIDER(wanted), NULL);
    g_object_set_data(G_OBJECT(view), DATA_KEYWORDS, wanted);
}

void
photon_completion_detach(PhotonCompletion *completion, PhotonDocument *doc) {
    GtkSourceView *view = photon_document_get_view(doc);
    if (g_object_get_data(G_OBJECT(view), DATA_WORDS_ON) == NULL) return;
    gtk_source_completion_words_unregister(completion->words, GTK_TEXT_BUFFER(photon_document_get_buffer(doc)));
    g_object_set_data(G_OBJECT(view), DATA_WORDS_ON, NULL);
}
