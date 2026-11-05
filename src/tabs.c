/**
 * @author: Ahmad Odeh
 * @date: 5th Nov, 2026
 *
**/
/*
 * tabs: notebook of documents. See tabs.h.
 *
 * Each page is the document's own widget; the PhotonDocument* hangs off
 * it as object data, and the tab label widgets hang off the document's
 * view so the label can be refreshed when the document changes.
 */
#include "tabs.h"

#define DATA_DOC   "photon-doc"
#define DATA_LABEL "photon-tab-label"

struct PhotonTabs {
    GtkWidget          *notebook;
    PhotonTabsCallbacks cb;
};

static PhotonDocument *
doc_at(PhotonTabs *tabs, gint page) {
    GtkWidget *child = gtk_notebook_get_nth_page(GTK_NOTEBOOK(tabs->notebook), page);
    return child != NULL ? g_object_get_data(G_OBJECT(child), DATA_DOC) : NULL;
}

static gint
page_of(PhotonTabs *tabs, PhotonDocument *doc) {
    return gtk_notebook_page_num(GTK_NOTEBOOK(tabs->notebook), photon_document_get_widget(doc));
}

// "*main.lunar" when modified; full path as tooltip
static void
refresh_label(PhotonDocument *doc) {
    GtkWidget *label = g_object_get_data(G_OBJECT(photon_document_get_view(doc)), DATA_LABEL);
    if (label == NULL) return;

    gchar *title = photon_document_get_title(doc);
    gchar *text = g_strconcat(photon_document_is_dirty(doc) ? "*" : "", title, NULL);
    gtk_label_set_text(GTK_LABEL(label), text);
    gtk_widget_set_tooltip_text(gtk_widget_get_parent(label), photon_document_get_path(doc));
    g_free(text);
    g_free(title);
}

static void
on_doc_changed(PhotonDocument *doc, PhotonDocChange what, gpointer user_data) {
    PhotonTabs *tabs = user_data;
    refresh_label(doc);
    if (tabs->cb.doc_changed != NULL) tabs->cb.doc_changed(doc, what, tabs->cb.user_data);
}

static void
on_close_clicked(GtkButton *button, gpointer user_data) {
    PhotonTabs *tabs = user_data;
    PhotonDocument *doc = g_object_get_data(G_OBJECT(button), DATA_DOC);
    if (tabs->cb.close_requested != NULL) tabs->cb.close_requested(doc, tabs->cb.user_data);
}

static GtkWidget *
build_label(PhotonTabs *tabs, PhotonDocument *doc) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget *label = gtk_label_new(NULL);
    GtkWidget *close = gtk_button_new_with_label("×");

    gtk_button_set_relief(GTK_BUTTON(close), GTK_RELIEF_NONE);
    gtk_widget_set_focus_on_click(close, FALSE);
    gtk_style_context_add_class(gtk_widget_get_style_context(close), "photon-tab-close");
    g_object_set_data(G_OBJECT(close), DATA_DOC, doc);
    g_signal_connect(close, "clicked", G_CALLBACK(on_close_clicked), tabs);

    gtk_box_pack_start(GTK_BOX(box), label, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(box), close, FALSE, FALSE, 0);
    g_object_set_data(G_OBJECT(photon_document_get_view(doc)), DATA_LABEL, label);
    gtk_widget_show_all(box);
    return box;
}

static void
on_switch_page(GtkNotebook *notebook G_GNUC_UNUSED, GtkWidget *page, guint num G_GNUC_UNUSED, gpointer user_data) {
    PhotonTabs *tabs = user_data;
    PhotonDocument *doc = g_object_get_data(G_OBJECT(page), DATA_DOC);
    if (tabs->cb.switched != NULL) tabs->cb.switched(doc, tabs->cb.user_data);
}

PhotonTabs *
photon_tabs_new(const PhotonTabsCallbacks *callbacks) {
    PhotonTabs *tabs = g_new0(PhotonTabs, 1);
    tabs->cb = *callbacks;

    tabs->notebook = gtk_notebook_new();
    gtk_notebook_set_scrollable(GTK_NOTEBOOK(tabs->notebook), TRUE);
    gtk_notebook_set_show_border(GTK_NOTEBOOK(tabs->notebook), FALSE);
    gtk_style_context_add_class(gtk_widget_get_style_context(tabs->notebook), "photon-tabs");
    g_signal_connect_after(tabs->notebook, "switch-page", G_CALLBACK(on_switch_page), tabs);
    g_object_ref_sink(tabs->notebook);
    return tabs;
}

void
photon_tabs_free(PhotonTabs *tabs) {
    if (tabs == NULL) return;
    g_signal_handlers_disconnect_by_data(tabs->notebook, tabs);
    GList *docs = photon_tabs_list(tabs);
    for (GList *l = docs; l != NULL; l = l->next) photon_tabs_close(tabs, l->data);
    g_list_free(docs);
    g_object_unref(tabs->notebook);
    g_free(tabs);
}

GtkWidget *
photon_tabs_get_widget(PhotonTabs *tabs) {
    return tabs->notebook;
}

void
photon_tabs_add(PhotonTabs *tabs, PhotonDocument *doc) {
    GtkWidget *page = photon_document_get_widget(doc);
    g_object_set_data(G_OBJECT(page), DATA_DOC, doc);
    photon_document_set_listener(doc, on_doc_changed, tabs);

    gint index = gtk_notebook_append_page(GTK_NOTEBOOK(tabs->notebook), page, build_label(tabs, doc));
    gtk_notebook_set_tab_reorderable(GTK_NOTEBOOK(tabs->notebook), page, TRUE);
    refresh_label(doc);
    gtk_widget_show_all(page);
    gtk_notebook_set_current_page(GTK_NOTEBOOK(tabs->notebook), index);
}

void
photon_tabs_close(PhotonTabs *tabs, PhotonDocument *doc) {
    gint page = page_of(tabs, doc);
    if (page < 0) return;
    photon_document_set_listener(doc, NULL, NULL);
    gtk_notebook_remove_page(GTK_NOTEBOOK(tabs->notebook), page);
    photon_document_free(doc);
}

void
photon_tabs_select(PhotonTabs *tabs, PhotonDocument *doc) {
    gint page = page_of(tabs, doc);
    if (page >= 0) gtk_notebook_set_current_page(GTK_NOTEBOOK(tabs->notebook), page);
}

PhotonDocument *
photon_tabs_current(PhotonTabs *tabs) {
    gint page = gtk_notebook_get_current_page(GTK_NOTEBOOK(tabs->notebook));
    return page >= 0 ? doc_at(tabs, page) : NULL;
}

gint
photon_tabs_current_index(PhotonTabs *tabs) {
    return gtk_notebook_get_current_page(GTK_NOTEBOOK(tabs->notebook));
}

guint
photon_tabs_count(PhotonTabs *tabs) {
    return (guint)gtk_notebook_get_n_pages(GTK_NOTEBOOK(tabs->notebook));
}

GList *
photon_tabs_list(PhotonTabs *tabs) {
    GList *docs = NULL;
    for (gint i = gtk_notebook_get_n_pages(GTK_NOTEBOOK(tabs->notebook)) - 1; i >= 0; i--) {
        docs = g_list_prepend(docs, doc_at(tabs, i));
    }
    return docs;
}

PhotonDocument *
photon_tabs_find(PhotonTabs *tabs, const gchar *path) {
    if (path == NULL) return NULL;
    gchar *wanted = g_canonicalize_filename(path, NULL);
    PhotonDocument *found = NULL;

    for (gint i = 0; found == NULL && i < gtk_notebook_get_n_pages(GTK_NOTEBOOK(tabs->notebook)); i++) {
        PhotonDocument *doc = doc_at(tabs, i);
        const gchar *doc_path = photon_document_get_path(doc);
        if (doc_path == NULL) continue;
        gchar *canonical = g_canonicalize_filename(doc_path, NULL);
        if (g_strcmp0(canonical, wanted) == 0) found = doc;
        g_free(canonical);
    }
    g_free(wanted);
    return found;
}
