/**
 * @author: Ahmad Odeh
 * @date: 27th Sep, 2026
 * 
**/


#include <gtk/gtk.h>
#include <gtksourceview/gtksource.h>
#include <string.h>


// ---------------------------------------------------------------------
// Forward declarations
// ---------------------------------------------------------------------
static void load_file(GtkSourceBuffer *buffer, const gchar *filename);
static void save_file(GtkSourceBuffer *buffer, const gchar *filename);
static void update_title(const gchar *filename);


// ---------------------------------------------------------------------
// Globals
// ---------------------------------------------------------------------
static gchar *current_filename = NULL;
static GtkWidget *window = NULL;
static GtkSourceBuffer *g_source_buffer = NULL; // lets tree callbacks load into the editor

enum {
    COL_NAME,
    COL_PATH,
    COL_IS_DIR,
    NUM_COLS
};

static GtkTreeStore *tree_store = NULL;
static GtkWidget *tree_view = NULL;


// ---------------------------------------------------------------------
// File tree sidebar
// ---------------------------------------------------------------------
static void
populate_dir(GtkTreeStore *store, GtkTreeIter *parent, const gchar *path) {
    GDir *dir;
    const gchar *name;
    GError *error = NULL;

    dir = g_dir_open(path, 0, &error);
    if (dir == NULL) {
        g_printerr("Could not open dir %s: %s\n", path, error->message);
        g_error_free(error);
        return;
    }

    while ((name = g_dir_read_name(dir)) != NULL) {
        gchar *full_path = g_build_filename(path, name, NULL);
        gboolean is_dir = g_file_test(full_path, G_FILE_TEST_IS_DIR);

        // Skip build artifacts so they never show up (and can't be clicked)
        if (!is_dir && (g_strcmp0(name, "Photon") == 0 || g_str_has_suffix(name, ".o"))) {
            g_free(full_path);
            continue;
        }

        GtkTreeIter iter;

        gtk_tree_store_append(store, &iter, parent);
        gtk_tree_store_set(store, &iter,
            COL_NAME, name,
            COL_PATH, full_path,
            COL_IS_DIR, is_dir,
            -1);

        if (is_dir) {
            GtkTreeIter dummy;
            gtk_tree_store_append(store, &dummy, &iter);
            gtk_tree_store_set(store, &dummy, COL_NAME, "...", COL_PATH, "", COL_IS_DIR, FALSE, -1);
        }

        g_free(full_path);
    }

    g_dir_close(dir);
}

static void
on_row_expanded(GtkTreeView *tv, GtkTreeIter *iter, GtkTreePath *tree_path, gpointer user_data) {
    GtkTreeIter child;
    gchar *path;
    gboolean is_dir;

    gtk_tree_model_get(GTK_TREE_MODEL(tree_store), iter, COL_PATH, &path, COL_IS_DIR, &is_dir, -1);

    if (!is_dir) { g_free(path); return; }

    // If the only child is the dummy placeholder, replace it with real contents
    if (gtk_tree_model_iter_children(GTK_TREE_MODEL(tree_store), &child, iter)) {
        gchar *child_path;
        gtk_tree_model_get(GTK_TREE_MODEL(tree_store), &child, COL_PATH, &child_path, -1);
        if (g_strcmp0(child_path, "") == 0) {
            gtk_tree_store_remove(tree_store, &child);
            populate_dir(tree_store, iter, path);
        }
        g_free(child_path);
    }

    g_free(path);
}

static void
on_row_activated(GtkTreeView *tv, GtkTreePath *tree_path, GtkTreeViewColumn *column, gpointer user_data) {
    GtkTreeIter iter;
    gchar *path;
    gboolean is_dir;

    if (!gtk_tree_model_get_iter(GTK_TREE_MODEL(tree_store), &iter, tree_path)) return;
    gtk_tree_model_get(GTK_TREE_MODEL(tree_store), &iter, COL_PATH, &path, COL_IS_DIR, &is_dir, -1);

    if (!is_dir && g_source_buffer != NULL) {
        load_file(g_source_buffer, path);
    }

    g_free(path);
}

static GtkWidget *
build_sidebar(const gchar *root_path) {
    GtkWidget *scroll;
    GtkCellRenderer *renderer;
    GtkTreeViewColumn *column;

    tree_store = gtk_tree_store_new(NUM_COLS, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_BOOLEAN);
    populate_dir(tree_store, NULL, root_path);

    tree_view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(tree_store));
    gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(tree_view), FALSE);

    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes("Name", renderer, "text", COL_NAME, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(tree_view), column);

    g_signal_connect(tree_view, "row-expanded", G_CALLBACK(on_row_expanded), NULL);
    g_signal_connect(tree_view, "row-activated", G_CALLBACK(on_row_activated), NULL);

    scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_container_add(GTK_CONTAINER(scroll), tree_view);
    gtk_widget_set_size_request(scroll, 220, -1);

    return scroll;
}


// ---------------------------------------------------------------------
// File load / save
// ---------------------------------------------------------------------
static void
update_title(const gchar *filename) {
    gchar *basename;
    gchar *title;

    if (filename == NULL) {
        gtk_window_set_title(GTK_WINDOW(window), "Photon");
        return;
    }

    basename = g_path_get_basename(filename);
    title    = g_strdup_printf("%s - Photon", basename);
    gtk_window_set_title(GTK_WINDOW(window), title);
    g_free(basename);
    g_free(title);
}

static void
load_file(GtkSourceBuffer *buffer, const gchar *filename) {
    gchar *contents;
    gsize length;
    GError *error = NULL;

    if (!g_file_get_contents(filename, &contents, &length, &error)) {
        g_printerr("Failed to load file: %s\n", error->message);
        g_error_free(error);
        return;
    }

    // Refuse to load binary/non-UTF-8 content — prevents the
    // gtk_text_buffer_emit_insert assertion crash on ELF binaries, images, etc.
    if (!g_utf8_validate(contents, length, NULL)) {
        g_printerr("Refusing to open '%s': not a valid UTF-8 text file (likely binary).\n", filename);
        g_free(contents);
        return;
    }

    GtkSourceLanguageManager *lang_manager = gtk_source_language_manager_get_default();
    GtkSourceLanguage *language = gtk_source_language_manager_guess_language(lang_manager, filename, NULL);

    if (language != NULL) {
        gtk_source_buffer_set_language(buffer, language);
    } else {
        gtk_source_buffer_set_language(buffer, NULL);
    }

    gtk_source_buffer_begin_not_undoable_action(buffer);
    gtk_text_buffer_set_text(GTK_TEXT_BUFFER(buffer), contents, length);
    gtk_source_buffer_end_not_undoable_action(buffer);

    g_free(contents);

    gchar *new_filename = g_strdup(filename);
    g_free(current_filename);
    current_filename = new_filename;

    update_title(current_filename);
}

static void
save_file(GtkSourceBuffer *buffer, const gchar *filename) {
    GtkTextIter start, end;
    gchar *text;
    GError *error = NULL;

    gtk_text_buffer_get_start_iter(GTK_TEXT_BUFFER(buffer), &start);
    gtk_text_buffer_get_end_iter(GTK_TEXT_BUFFER(buffer), &end);
    text = gtk_text_buffer_get_text(GTK_TEXT_BUFFER(buffer), &start, &end, FALSE);

    if (!g_file_set_contents(filename, text, -1, &error)) {
        g_printerr("Failed to save file: %s\n", error->message);
        g_error_free(error);
    } else {
        // Duplicate BEFORE freeing — filename may be the same pointer as
        // current_filename (e.g. when saving an already-open file), so
        // freeing first and then reading filename would read freed memory.
        gchar *new_filename = g_strdup(filename);
        g_free(current_filename);
        current_filename = new_filename;
        update_title(current_filename);
    }

    g_free(text);
}


// ---------------------------------------------------------------------
// Menu callbacks
// ---------------------------------------------------------------------
static void
on_open_clicked(GtkWidget *widget, gpointer user_data) {
    GtkSourceBuffer *buffer = GTK_SOURCE_BUFFER(user_data);
    GtkWidget *dialog;

    dialog = gtk_file_chooser_dialog_new("Open File",
        GTK_WINDOW(window), GTK_FILE_CHOOSER_ACTION_OPEN,
        "_Cancel", GTK_RESPONSE_CANCEL,
        "_Open", GTK_RESPONSE_ACCEPT,
        NULL);

    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        gchar *filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
        load_file(buffer, filename);
        g_free(filename);
    }

    gtk_widget_destroy(dialog);
}

static void
on_save_clicked(GtkWidget *widget, gpointer user_data) {
    GtkSourceBuffer *buffer = GTK_SOURCE_BUFFER(user_data);

    if (current_filename != NULL) {
        save_file(buffer, current_filename);
        return;
    }

    // No filename yet — fall through to Save As behavior
    GtkWidget *dialog = gtk_file_chooser_dialog_new("Save File",
        GTK_WINDOW(window), GTK_FILE_CHOOSER_ACTION_SAVE,
        "_Cancel", GTK_RESPONSE_CANCEL,
        "_Save", GTK_RESPONSE_ACCEPT,
        NULL);
    gtk_file_chooser_set_do_overwrite_confirmation(GTK_FILE_CHOOSER(dialog), TRUE);

    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        gchar *filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
        save_file(buffer, filename);
        g_free(filename);
    }

    gtk_widget_destroy(dialog);
}


// ---------------------------------------------------------------------
// main
// ---------------------------------------------------------------------
int main(int argc, char *argv[]) {
    GtkWidget *source_view;
    GtkWidget *scrolled_window;
    GtkSourceBuffer *source_buffer;
    GtkSourceStyleSchemeManager *scheme_manager;
    GtkSourceStyleScheme *scheme;

    gchar *sidebar_root = NULL;
    gchar *file_to_open = NULL;

    // Initialize GTK
    gtk_init(&argc, &argv);

    // Resolve command-line argument: `photon file.txt` or `photon some_dir/`
    if (argc > 1) {
        gchar *arg_path = g_canonicalize_filename(argv[1], NULL);

        if (g_file_test(arg_path, G_FILE_TEST_IS_DIR)) {
            sidebar_root = arg_path; // ownership transferred, freed later
        } else {
            // Existing file OR a not-yet-created file — either way, root the
            // sidebar at its parent directory and try to load it as a file
            file_to_open = arg_path; // ownership transferred, freed later
            sidebar_root = g_path_get_dirname(arg_path);
        }
    }

    if (sidebar_root == NULL) {
        sidebar_root = g_get_current_dir();
    }

    // Create the main application window
    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_default_size(GTK_WINDOW(window), 1000, 650);
    g_signal_connect(window, "delete-event", G_CALLBACK(gtk_main_quit), NULL);


    // Icon
    GError *icon_error = NULL;
    if (!gtk_window_set_icon_from_file(GTK_WINDOW(window), "icons/photon.png", &icon_error)) {
        g_printerr("Failed to load icon: %s\n", icon_error->message);
        g_error_free(icon_error);
    }

    // Editor pane
    scrolled_window = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

    // No language set at startup — it's picked per-file in load_file() via guess_language
    source_buffer = gtk_source_buffer_new(NULL);
    g_source_buffer = source_buffer; // expose to sidebar callbacks

    // Apply the VS Code-style dark scheme
    scheme_manager = gtk_source_style_scheme_manager_get_default();
    scheme = gtk_source_style_scheme_manager_get_scheme(scheme_manager, "vscode-dark");
    if (scheme != NULL) {
        gtk_source_buffer_set_style_scheme(source_buffer, scheme);
    } else {
        g_printerr("Style scheme 'vscode-dark' not found, gutter will stay default.\n");
    }

    source_view = gtk_source_view_new_with_buffer(source_buffer);
    gtk_container_add(GTK_CONTAINER(scrolled_window), source_view);
    gtk_source_view_set_show_line_numbers(GTK_SOURCE_VIEW(source_view), TRUE);

    // Font — CSS with a fallback chain so it never lands on an ugly default
    GtkCssProvider *font_provider = gtk_css_provider_new();
    gtk_css_provider_load_from_data(font_provider,
        "textview {"
        "  font-family: 'JetBrains Mono', 'Fira Code', 'Cascadia Code',"
        "               'Source Code Pro', 'DejaVu Sans Mono', monospace;"
        "  font-size: 11pt;"
        "}",
        -1, NULL);
    gtk_style_context_add_provider(gtk_widget_get_style_context(source_view),
        GTK_STYLE_PROVIDER(font_provider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref(font_provider);

    // vbox holds the menu bar + editor, and becomes the RIGHT pane of the split
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);

    // Menu bar
    GtkWidget *menu_bar  = gtk_menu_bar_new();
    GtkWidget *file_menu = gtk_menu_new();
    GtkWidget *file_item = gtk_menu_item_new_with_label("File");
    GtkWidget *open_item = gtk_menu_item_new_with_label("Open");
    GtkWidget *save_item = gtk_menu_item_new_with_label("Save");

    gtk_menu_shell_append(GTK_MENU_SHELL(file_menu), open_item);
    gtk_menu_shell_append(GTK_MENU_SHELL(file_menu), save_item);
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(file_item), file_menu);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu_bar), file_item);

    g_signal_connect(open_item, "activate", G_CALLBACK(on_open_clicked), source_buffer);
    g_signal_connect(save_item, "activate", G_CALLBACK(on_save_clicked), source_buffer);

    gtk_box_pack_start(GTK_BOX(vbox), menu_bar, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), scrolled_window, TRUE, TRUE, 0);

    // Sidebar (left pane) — built ONCE, rooted at sidebar_root
    // (which is either the given directory, the parent of a given file, or cwd)
    GtkWidget *sidebar = build_sidebar(sidebar_root);
    g_free(sidebar_root);

    // Split: sidebar on the left, editor (vbox) on the right
    GtkWidget *hpaned = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_paned_pack1(GTK_PANED(hpaned), sidebar, FALSE, FALSE);
    gtk_paned_pack2(GTK_PANED(hpaned), vbox, TRUE, FALSE);
    gtk_paned_set_position(GTK_PANED(hpaned), 220);

    gtk_container_add(GTK_CONTAINER(window), hpaned);

    // Load the requested file (if any) — this sets the title itself.
    // Only fall back to the default "Photon" title if nothing was requested.
    if (file_to_open != NULL) {
        if (g_file_test(file_to_open, G_FILE_TEST_EXISTS)) {
            load_file(source_buffer, file_to_open);
        } else {
            // New file: not on disk yet, but set it as current_filename so
            // Save (not Save As) writes here directly
            current_filename = g_strdup(file_to_open);
            update_title(current_filename);
        }
        g_free(file_to_open);
    } else {
        update_title(NULL);
    }

    // Show all widgets
    gtk_widget_show_all(window);

    // Start the GTK main loop
    gtk_main();

    return 0;
}