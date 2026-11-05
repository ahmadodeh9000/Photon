/**
 * @author: Ahmad Odeh
 * @date: 5th Nov, 2026
 *
**/
/*
 * sidebar: lazy file tree. See sidebar.h.
 */
#include "sidebar.h"

enum {
    COL_NAME,
    COL_PATH,     // "" marks the dummy child of a folder that isn't read yet
    COL_IS_DIR,
    NUM_COLS
};

struct PhotonSidebar {
    gchar                *root_dir;
    GtkWidget            *root;    // scrolled window
    GtkWidget            *view;
    GtkTreeStore         *store;
    PhotonSidebarOpenFunc on_open;
    gpointer              user_data;
};

// Folders first, then files, both alphabetical (case-insensitive)
static gint
sort_rows(GtkTreeModel *model, GtkTreeIter *a, GtkTreeIter *b, gpointer user_data G_GNUC_UNUSED) {
    gchar *name_a, *name_b;
    gboolean dir_a, dir_b;
    gint result;

    gtk_tree_model_get(model, a, COL_NAME, &name_a, COL_IS_DIR, &dir_a, -1);
    gtk_tree_model_get(model, b, COL_NAME, &name_b, COL_IS_DIR, &dir_b, -1);

    if (dir_a != dir_b) {
        result = dir_a ? -1 : 1;
    } else {
        gchar *key_a = g_utf8_casefold(name_a, -1);
        gchar *key_b = g_utf8_casefold(name_b, -1);
        result = g_utf8_collate(key_a, key_b);
        g_free(key_a);
        g_free(key_b);
    }

    g_free(name_a);
    g_free(name_b);
    return result;
}

// Noise and build artifacts nobody wants (or can open) in a sidebar
static gboolean
is_hidden_entry(const gchar *name, gboolean is_dir) {
    if (g_strcmp0(name, ".git") == 0 || g_strcmp0(name, ".DS_Store") == 0) return TRUE;
    if (!is_dir && (g_strcmp0(name, "Photon") == 0 || g_str_has_suffix(name, ".o"))) return TRUE;
    return FALSE;
}

static void
populate_dir(GtkTreeStore *store, GtkTreeIter *parent, const gchar *path) {
    GError *error = NULL;
    GDir *dir = g_dir_open(path, 0, &error);
    if (dir == NULL) {
        g_printerr("Could not open dir %s: %s\n", path, error->message);
        g_error_free(error);
        return;
    }

    const gchar *name;
    while ((name = g_dir_read_name(dir)) != NULL) {
        gchar *full_path = g_build_filename(path, name, NULL);
        gboolean is_dir = g_file_test(full_path, G_FILE_TEST_IS_DIR);

        if (!is_hidden_entry(name, is_dir)) {
            GtkTreeIter iter;
            gtk_tree_store_append(store, &iter, parent);
            gtk_tree_store_set(store, &iter, COL_NAME, name, COL_PATH, full_path, COL_IS_DIR, is_dir, -1);

            if (is_dir) {
                // Dummy child so the row shows an expander; replaced on first expand
                GtkTreeIter dummy;
                gtk_tree_store_append(store, &dummy, &iter);
                gtk_tree_store_set(store, &dummy, COL_NAME, "...", COL_PATH, "", COL_IS_DIR, FALSE, -1);
            }
        }
        g_free(full_path);
    }
    g_dir_close(dir);
}

static void
on_row_expanded(GtkTreeView *tv G_GNUC_UNUSED, GtkTreeIter *iter,
                GtkTreePath *tree_path G_GNUC_UNUSED, gpointer user_data) {
    PhotonSidebar *sidebar = user_data;
    GtkTreeModel *model = GTK_TREE_MODEL(sidebar->store);
    GtkTreeIter child;
    gchar *path;
    gboolean is_dir;

    gtk_tree_model_get(model, iter, COL_PATH, &path, COL_IS_DIR, &is_dir, -1);

    // Only the dummy placeholder there? Replace it with the real contents.
    if (is_dir && gtk_tree_model_iter_children(model, &child, iter)) {
        gchar *child_path;
        gtk_tree_model_get(model, &child, COL_PATH, &child_path, -1);
        if (g_strcmp0(child_path, "") == 0) {
            gtk_tree_store_remove(sidebar->store, &child);
            populate_dir(sidebar->store, iter, path);
        }
        g_free(child_path);
    }
    g_free(path);
}

static void
on_row_activated(GtkTreeView *tv, GtkTreePath *tree_path,
                 GtkTreeViewColumn *column G_GNUC_UNUSED, gpointer user_data) {
    PhotonSidebar *sidebar = user_data;
    GtkTreeIter iter;
    gchar *path;
    gboolean is_dir;

    if (!gtk_tree_model_get_iter(GTK_TREE_MODEL(sidebar->store), &iter, tree_path)) return;
    gtk_tree_model_get(GTK_TREE_MODEL(sidebar->store), &iter, COL_PATH, &path, COL_IS_DIR, &is_dir, -1);

    if (is_dir) {
        // Double-click on a folder toggles it, like every other file tree
        if (gtk_tree_view_row_expanded(tv, tree_path)) {
            gtk_tree_view_collapse_row(tv, tree_path);
        } else {
            gtk_tree_view_expand_row(tv, tree_path, FALSE);
        }
    } else if (sidebar->on_open != NULL) {
        sidebar->on_open(path, sidebar->user_data);
    }
    g_free(path);
}

PhotonSidebar *
photon_sidebar_new(const gchar *root_dir, PhotonSidebarOpenFunc on_open, gpointer user_data) {
    PhotonSidebar *sidebar = g_new0(PhotonSidebar, 1);
    sidebar->root_dir = g_strdup(root_dir);
    sidebar->on_open = on_open;
    sidebar->user_data = user_data;

    sidebar->store = gtk_tree_store_new(NUM_COLS, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_BOOLEAN);
    gtk_tree_sortable_set_default_sort_func(GTK_TREE_SORTABLE(sidebar->store), sort_rows, NULL, NULL);
    gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(sidebar->store),
        GTK_TREE_SORTABLE_DEFAULT_SORT_COLUMN_ID, GTK_SORT_ASCENDING);
    populate_dir(sidebar->store, NULL, root_dir);

    sidebar->view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(sidebar->store));
    gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(sidebar->view), FALSE);

    GtkCellRenderer *renderer = gtk_cell_renderer_text_new();
    GtkTreeViewColumn *column = gtk_tree_view_column_new_with_attributes("Name", renderer, "text", COL_NAME, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(sidebar->view), column);

    g_signal_connect(sidebar->view, "row-expanded", G_CALLBACK(on_row_expanded), sidebar);
    g_signal_connect(sidebar->view, "row-activated", G_CALLBACK(on_row_activated), sidebar);

    sidebar->root = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sidebar->root),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_container_add(GTK_CONTAINER(sidebar->root), sidebar->view);
    gtk_widget_set_size_request(sidebar->root, 220, -1);
    g_object_ref_sink(sidebar->root);

    return sidebar;
}

void
photon_sidebar_free(PhotonSidebar *sidebar) {
    if (sidebar == NULL) return;
    g_object_unref(sidebar->root);
    g_object_unref(sidebar->store);
    g_free(sidebar->root_dir);
    g_free(sidebar);
}

GtkWidget *
photon_sidebar_get_widget(PhotonSidebar *sidebar) {
    return sidebar->root;
}

void
photon_sidebar_toggle(PhotonSidebar *sidebar) {
    gtk_widget_set_visible(sidebar->root, !gtk_widget_get_visible(sidebar->root));
}

// ---------------------------------------------------------------------
// Refresh
// ---------------------------------------------------------------------
static void
remember_expanded(GtkTreeView *view, GtkTreePath *tree_path, gpointer user_data) {
    GPtrArray *paths = user_data;
    GtkTreeModel *model = gtk_tree_view_get_model(view);
    GtkTreeIter iter;
    gchar *path;
    if (!gtk_tree_model_get_iter(model, &iter, tree_path)) return;
    gtk_tree_model_get(model, &iter, COL_PATH, &path, -1);
    g_ptr_array_add(paths, path);
}

// Row whose COL_PATH is `path`, searching only rows already listed
static gboolean
find_row(GtkTreeModel *model, GtkTreeIter *parent, const gchar *path, GtkTreeIter *found) {
    GtkTreeIter iter;
    if (!gtk_tree_model_iter_children(model, &iter, parent)) return FALSE;
    do {
        gchar *row_path;
        gtk_tree_model_get(model, &iter, COL_PATH, &row_path, -1);
        gboolean match = g_strcmp0(row_path, path) == 0;
        gboolean inside = !match && *row_path != '\0' && g_str_has_prefix(path, row_path);
        g_free(row_path);
        if (match) {
            *found = iter;
            return TRUE;
        }
        if (inside && find_row(model, &iter, path, found)) return TRUE;
    } while (gtk_tree_model_iter_next(model, &iter));
    return FALSE;
}

void
photon_sidebar_refresh(PhotonSidebar *sidebar) {
    GtkTreeView *view = GTK_TREE_VIEW(sidebar->view);
    GPtrArray *expanded = g_ptr_array_new_with_free_func(g_free);
    gtk_tree_view_map_expanded_rows(view, remember_expanded, expanded);   // parents come first

    gtk_tree_store_clear(sidebar->store);
    populate_dir(sidebar->store, NULL, sidebar->root_dir);

    for (guint i = 0; i < expanded->len; i++) {
        GtkTreeIter iter;
        if (!find_row(GTK_TREE_MODEL(sidebar->store), NULL, g_ptr_array_index(expanded, i), &iter)) continue;
        GtkTreePath *tree_path = gtk_tree_model_get_path(GTK_TREE_MODEL(sidebar->store), &iter);
        gtk_tree_view_expand_row(view, tree_path, FALSE);   // reads the folder via on_row_expanded
        gtk_tree_path_free(tree_path);
    }
    g_ptr_array_free(expanded, TRUE);
}
