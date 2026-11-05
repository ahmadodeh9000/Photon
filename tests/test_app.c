/**
 * @author: Ahmad Odeh
 * @date: 5th Nov, 2026
 *
**/
/*
 * test_app: tabs and the session through the real PhotonApp: opening a
 * file twice, reusing a blank tab, closing the last tab, reopening files
 * on the next launch, changing the language. Uses a throwaway config dir.
 */
#include <glib/gstdio.h>
#include <gtk/gtk.h>

#include "../src/app.h"
#include "../src/languages.h"
#include "check.h"

static gchar *
write_file(const gchar *dir, const gchar *name, const gchar *text) {
    gchar *path = g_build_filename(dir, name, NULL);
    g_file_set_contents(path, text, -1, NULL);
    return path;
}

static PhotonApp *
start(PhotonTheme *theme, const gchar *dir, gboolean session) {
    PhotonAppOptions options = { .root_dir = dir, .theme = theme, .use_session = session };
    return photon_app_new(&options);
}

static void
test_tabs(PhotonTheme *theme, const gchar *dir) {
    gchar *a = write_file(dir, "a.lunar", "print 1;\n");
    gchar *b = write_file(dir, "b.py", "print(2)\n");
    PhotonApp *app = start(theme, dir, FALSE);

    CHECK(photon_tabs_count(app->tabs) == 1);                       // blank Lunar tab
    CHECK_STR(photon_document_get_language(photon_app_current_doc(app))->id, "lunar");

    CHECK(photon_app_open_file(app, a));
    CHECK(photon_tabs_count(app->tabs) == 1);                       // reused the blank tab
    CHECK(photon_app_open_file(app, b));
    CHECK(photon_tabs_count(app->tabs) == 2);
    CHECK(photon_app_open_file(app, a));                            // already open: switch
    CHECK(photon_tabs_count(app->tabs) == 2);
    CHECK_STR(photon_document_get_path(photon_app_current_doc(app)), a);

    gchar *a_dots = g_build_filename(dir, ".", "a.lunar", NULL);    // same file, other spelling
    CHECK(photon_app_open_file(app, a_dots));
    CHECK(photon_tabs_count(app->tabs) == 2);
    g_free(a_dots);

    photon_app_set_autosave(app, FALSE);
    CHECK(photon_app_close_document(app, photon_app_current_doc(app)));
    CHECK(photon_app_close_document(app, photon_app_current_doc(app)));
    CHECK(photon_tabs_count(app->tabs) == 1);                       // a fresh blank tab replaces the last
    CHECK(photon_document_is_blank(photon_app_current_doc(app)));

    photon_app_free(app);
    g_free(a);
    g_free(b);
}

static void
test_session(PhotonTheme *theme, const gchar *dir) {
    gchar *a = write_file(dir, "s1.c", "int main(void) { return 0; }\n");
    gchar *b = write_file(dir, "s2.lunar", "let x = 1;\n");

    PhotonApp *app = start(theme, dir, TRUE);
    photon_app_open_file(app, a);
    photon_app_open_file(app, b);
    photon_app_open_file(app, a);       // a is current when Photon quits
    photon_app_free(app);

    app = start(theme, dir, TRUE);      // next launch
    CHECK(photon_tabs_count(app->tabs) == 2);
    CHECK_STR(photon_document_get_path(photon_app_current_doc(app)), a);
    photon_app_free(app);

    g_unlink(b);                         // a file deleted since is skipped
    app = start(theme, dir, TRUE);
    CHECK(photon_tabs_count(app->tabs) == 1);
    photon_app_free(app);
    g_free(a);
    g_free(b);
}

// Names of the completion providers on a document's view, e.g. "Words,Lunar"
static gchar *
provider_names(PhotonDocument *doc) {
    GtkSourceCompletion *completion = gtk_source_view_get_completion(photon_document_get_view(doc));
    GString *names = g_string_new(NULL);
    for (GList *l = gtk_source_completion_get_providers(completion); l != NULL; l = l->next) {
        gchar *name = gtk_source_completion_provider_get_name(l->data);
        g_string_append_printf(names, "%s%s", names->len ? "," : "", name);
        g_free(name);
    }
    return g_string_free(names, FALSE);
}

static void
test_language_picker(PhotonTheme *theme, const gchar *dir) {
    PhotonApp *app = start(theme, dir, FALSE);
    PhotonDocument *doc = photon_app_current_doc(app);
    gchar *names = provider_names(doc);
    CHECK_STR(names, "Words,Lunar");                 // buffer words + Lunar keywords
    g_free(names);

    photon_document_set_language(doc, photon_languages_find("python"));
    names = provider_names(doc);
    CHECK_STR(names, "Words,Python");                // keyword list follows the language
    g_free(names);
    CHECK_STR(photon_document_get_language(doc)->id, "python");
    gchar *title = photon_document_get_title(doc);
    CHECK_STR(title, "Untitled.py");
    g_free(title);
    photon_app_free(app);
}

// Is `name` a top-level row in the sidebar? (reads the real tree view)
static gboolean
sidebar_lists(PhotonApp *app, const gchar *name) {
    GtkWidget *scroll = photon_sidebar_get_widget(app->sidebar);
    GtkTreeModel *model = gtk_tree_view_get_model(GTK_TREE_VIEW(gtk_bin_get_child(GTK_BIN(scroll))));
    GtkTreeIter iter;
    gboolean found = FALSE;
    for (gboolean ok = gtk_tree_model_get_iter_first(model, &iter); ok && !found; ok = gtk_tree_model_iter_next(model, &iter)) {
        gchar *row;
        gtk_tree_model_get(model, &iter, 0, &row, -1);
        found = g_strcmp0(row, name) == 0;
        g_free(row);
    }
    return found;
}

static void
test_sidebar_refresh(PhotonTheme *theme, const gchar *dir) {
    PhotonApp *app = start(theme, dir, FALSE);
    CHECK(!sidebar_lists(app, "fresh.lunar"));

    gchar *path = g_build_filename(dir, "fresh.lunar", NULL);
    PhotonDocument *doc = photon_app_current_doc(app);
    photon_document_set_path(doc, path);
    CHECK(photon_app_save(app, doc));
    CHECK(sidebar_lists(app, "fresh.lunar"));        // new file shows up after saving

    photon_app_free(app);
    g_free(path);
}

int
main(int argc, char *argv[]) {
    gchar *dir = g_dir_make_tmp("photon-test-XXXXXX", NULL);
    gchar *config = g_build_filename(dir, "config", NULL);
    g_setenv("XDG_CONFIG_HOME", config, TRUE);   // never touch the real settings.ini
    gtk_init(&argc, &argv);
    photon_languages_init(".");
    PhotonTheme *theme = photon_theme_new(".");

    test_tabs(theme, dir);
    test_session(theme, dir);
    test_language_picker(theme, dir);
    test_sidebar_refresh(theme, dir);

    photon_theme_free(theme);
    g_free(config);
    g_free(dir);
    return check_finish("test_app");
}
