/**
 * @author: Ahmad Odeh
 * @date: 27th Sep, 2026
 *
**/
/*
I LOVE PHOTON LOL
MADE WITH LOVE
*/

#include <stdio.h>
#include <stdlib.h>


#include <gtk/gtk.h>
#include <gtksourceview/gtksource.h>
#include <string.h>
#include <vte/vte.h>



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
static GtkWidget *sidebar = NULL;                // left pane, toggled from the View menu
static GtkWidget *terminal_pane = NULL;          // bottom pane, toggled from the View menu
static GtkWidget *terminal_widget = NULL;        // the VteTerminal inside terminal_pane
static gchar *terminal_cwd = NULL;               // working dir used when (re)spawning the shell
static gboolean terminal_dead = FALSE;           // TRUE after the shell exits (e.g. Ctrl+D)
static GtkSourceBuffer *g_source_buffer = NULL;  // lets tree callbacks load into the editor
static guint autosave_timer_id = 0;

enum {
    COL_NAME,
    COL_PATH,
    COL_IS_DIR,
    NUM_COLS
};

static GtkTreeStore *tree_store = NULL;
static GtkWidget *tree_view = NULL;


// ---------------------------------------------------------------------
// Theme: dark chrome so the menu bar / sidebar / dialogs match the editor
// ---------------------------------------------------------------------
static void
apply_theme(void) {
    GtkSettings *settings = gtk_settings_get_default();
    g_object_set(settings, "gtk-application-prefer-dark-theme", TRUE, NULL);

    const gchar *css =
        "window, .background { background-color: #1e1e1e; color: #cccccc; }"

        "menubar { background-color: #333333; color: #cccccc; padding: 0; }"
        "menubar > menuitem { color: #cccccc; padding: 4px 10px; }"
        "menubar > menuitem:hover { background-color: #094771; color: #ffffff; }"

        "menu, .menu { background-color: #252526; color: #cccccc;"
        "              border: 1px solid #454545; }"
        "menuitem { color: #cccccc; }"
        "menuitem:hover { background-color: #094771; color: #ffffff; }"
        "separator { background-color: #454545; }"

        "treeview.view { background-color: #252526; color: #cccccc; }"
        "treeview.view:hover { background-color: #2a2d2e; }"
        "treeview.view:selected, treeview.view:selected:focus {"
        "    background-color: #094771; color: #ffffff; }"

        "paned > separator { background-image: image(#3c3c3c); }"

        "textview {"
        "  font-family: 'JetBrains Mono', 'Fira Code', 'Cascadia Code',"
        "               'Source Code Pro', 'DejaVu Sans Mono', Menlo, monospace;"
        "  font-size: 11pt;"
        "}";

    GtkCssProvider *provider = gtk_css_provider_new();
    GError *error = NULL;

    if (!gtk_css_provider_load_from_data(provider, css, -1, &error)) {
        g_printerr("Theme CSS failed to load: %s\n", error->message);
        g_error_free(error);
    } else {
        gtk_style_context_add_provider_for_screen(
            gdk_screen_get_default(),
            GTK_STYLE_PROVIDER(provider),
            GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    }

    g_object_unref(provider);
}


// ---------------------------------------------------------------------
// File tree sidebar
// ---------------------------------------------------------------------

// Folders first, then files, both alphabetical (case-insensitive)
static gint
tree_sort_func(GtkTreeModel *model, GtkTreeIter *a, GtkTreeIter *b, gpointer user_data) {
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
        // Noise nobody wants in a sidebar
        if (g_strcmp0(name, ".git") == 0 || g_strcmp0(name, ".DS_Store") == 0) {
            continue;
        }

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
            // dummy child so the row shows an expander; replaced on real expand
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

    if (is_dir) {
        // Double-click on a folder toggles it, like every other file tree
        if (gtk_tree_view_row_expanded(tv, tree_path)) {
            gtk_tree_view_collapse_row(tv, tree_path);
        } else {
            gtk_tree_view_expand_row(tv, tree_path, FALSE);
        }
    } else if (g_source_buffer != NULL) {
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

    gtk_tree_sortable_set_default_sort_func(GTK_TREE_SORTABLE(tree_store), tree_sort_func, NULL, NULL);
    gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(tree_store),
        GTK_TREE_SORTABLE_DEFAULT_SORT_COLUMN_ID, GTK_SORT_ASCENDING);

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
// Terminal
// ---------------------------------------------------------------------
static void
spawn_shell(void) {
    gchar *shell = vte_get_user_shell();
    if (shell == NULL) shell = g_strdup("/bin/bash");

    gchar *argv_shell[] = { shell, NULL };

    vte_terminal_spawn_async(VTE_TERMINAL(terminal_widget),
        VTE_PTY_DEFAULT,
        terminal_cwd,
        argv_shell,
        NULL,           // envp
        G_SPAWN_DEFAULT,
        NULL, NULL,     // child setup
        NULL,           // child setup data destroy
        -1,             // timeout
        NULL,           // cancellable
        NULL, NULL);    // callback

    g_free(shell);
    terminal_dead = FALSE;
}

// Shell exited (Ctrl+D, `exit`, crash): hide the pane and remember it's dead.
// The next toggle spawns a fresh shell. Respawning here would loop forever
// if the shell dies instantly.
static void
on_terminal_child_exited(VteTerminal *term, gint status, gpointer user_data) {
    terminal_dead = TRUE;
    if (terminal_pane != NULL) gtk_widget_hide(terminal_pane);
}

static GtkWidget *
build_terminal(const gchar *working_dir) {
    terminal_widget = vte_terminal_new();
    terminal_cwd = g_strdup(working_dir);

    // Dark colors matching the editor theme
    GdkRGBA fg, bg;
    gdk_rgba_parse(&fg, "#cccccc");
    gdk_rgba_parse(&bg, "#1e1e1e");
    vte_terminal_set_colors(VTE_TERMINAL(terminal_widget), &fg, &bg, NULL, 0);

    g_signal_connect(terminal_widget, "child-exited",
                     G_CALLBACK(on_terminal_child_exited), NULL);

    spawn_shell();

    GtkWidget *scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_container_add(GTK_CONTAINER(scroll), terminal_widget);
    gtk_widget_set_size_request(scroll, -1, 200); // default height

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

// Pick the highlighting language from the filename (c.lang: *.c;*.h, lunar.lang: *.lunar;*.lun)
static void
apply_language(GtkSourceBuffer *buffer, const gchar *filename) {
    GtkSourceLanguageManager *lang_manager = gtk_source_language_manager_get_default();
    GtkSourceLanguage *language = gtk_source_language_manager_guess_language(lang_manager, filename, NULL);

    gtk_source_buffer_set_language(buffer, language); // NULL = plain text
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

    // Refuse to load binary/non-UTF-8 content prevents the
    // gtk_text_buffer_emit_insert assertion crash on ELF binaries, images, etc.
    if (!g_utf8_validate(contents, length, NULL)) {
        g_printerr("Refusing to open '%s': not a valid UTF-8 text file (likely binary).\n", filename);
        g_free(contents);
        return;
    }

    apply_language(buffer, filename);

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
        // Duplicate BEFORE freeing filename may be the same pointer as
        // current_filename (e.g. when saving an already-open file), so
        // freeing first and then reading filename would read freed memory.
        gchar *new_filename = g_strdup(filename);
        g_free(current_filename);
        current_filename = new_filename;

        apply_language(buffer, current_filename); // "Save As x.lunar" switches highlighting
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

    // Native dialog: real NSOpenPanel on macOS, GTK/portal dialog elsewhere
    GtkFileChooserNative *native = gtk_file_chooser_native_new("Open File",
        GTK_WINDOW(window), GTK_FILE_CHOOSER_ACTION_OPEN, "_Open", "_Cancel");

    if (gtk_native_dialog_run(GTK_NATIVE_DIALOG(native)) == GTK_RESPONSE_ACCEPT) {
        gchar *filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(native));
        if (filename != NULL) {
            load_file(buffer, filename);
            g_free(filename);
        }
    }

    g_object_unref(native);
}

static void
save_as(GtkSourceBuffer *buffer) {
    GtkFileChooserNative *native = gtk_file_chooser_native_new("Save File",
        GTK_WINDOW(window), GTK_FILE_CHOOSER_ACTION_SAVE, "_Save", "_Cancel");
    gtk_file_chooser_set_do_overwrite_confirmation(GTK_FILE_CHOOSER(native), TRUE);

    if (current_filename != NULL) {
        gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(native), current_filename);
    }

    if (gtk_native_dialog_run(GTK_NATIVE_DIALOG(native)) == GTK_RESPONSE_ACCEPT) {
        gchar *filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(native));
        if (filename != NULL) {
            save_file(buffer, filename);
            g_free(filename);
        }
    }

    g_object_unref(native);
}

static void
on_save_clicked(GtkWidget *widget, gpointer user_data) {
    GtkSourceBuffer *buffer = GTK_SOURCE_BUFFER(user_data);

    if (current_filename != NULL) {
        save_file(buffer, current_filename);
    } else {
        save_as(buffer);
    }
}

static void
on_save_as_clicked(GtkWidget *widget, gpointer user_data) {
    save_as(GTK_SOURCE_BUFFER(user_data));
}

static gboolean
autosave_tick(gpointer user_data) {
    GtkSourceBuffer *buffer = GTK_SOURCE_BUFFER(user_data);

    if (current_filename != NULL && gtk_text_buffer_get_modified(GTK_TEXT_BUFFER(buffer))) {
        save_file(buffer, current_filename);
        gtk_text_buffer_set_modified(GTK_TEXT_BUFFER(buffer), FALSE);
    }

    return G_SOURCE_CONTINUE; // keep firing every interval
}

static void
on_autosave_toggled(GtkCheckMenuItem *item, gpointer user_data) {
    GtkSourceBuffer *buffer = GTK_SOURCE_BUFFER(user_data);

    if (gtk_check_menu_item_get_active(item)) {
        if (autosave_timer_id == 0) {
            autosave_timer_id = g_timeout_add_seconds(30, autosave_tick, buffer);
        }
    } else {
        if (autosave_timer_id != 0) {
            g_source_remove(autosave_timer_id);
            autosave_timer_id = 0;
        }
    }
}

static void
on_quit_clicked(GtkWidget *widget, gpointer user_data) {
    gtk_main_quit();
}

static void
on_undo_clicked(GtkWidget *widget, gpointer user_data) {
    GtkSourceBuffer *buffer = GTK_SOURCE_BUFFER(user_data);
    if (gtk_source_buffer_can_undo(buffer)) gtk_source_buffer_undo(buffer);
}

static void
on_redo_clicked(GtkWidget *widget, gpointer user_data) {
    GtkSourceBuffer *buffer = GTK_SOURCE_BUFFER(user_data);
    if (gtk_source_buffer_can_redo(buffer)) gtk_source_buffer_redo(buffer);
}

// user_data is the GtkSourceView for the clipboard actions
static void
on_cut_clicked(GtkWidget *widget, gpointer user_data) {
    g_signal_emit_by_name(user_data, "cut-clipboard");
}

static void
on_copy_clicked(GtkWidget *widget, gpointer user_data) {
    g_signal_emit_by_name(user_data, "copy-clipboard");
}

static void
on_paste_clicked(GtkWidget *widget, gpointer user_data) {
    g_signal_emit_by_name(user_data, "paste-clipboard");
}

static void
on_select_all_clicked(GtkWidget *widget, gpointer user_data) {
    g_signal_emit_by_name(user_data, "select-all", TRUE);
}

static void
on_toggle_sidebar_clicked(GtkWidget *widget, gpointer user_data) {
    if (sidebar == NULL) return;
    gtk_widget_set_visible(sidebar, !gtk_widget_get_visible(sidebar));
}

static void
on_toggle_terminal_clicked(GtkWidget *widget, gpointer user_data) {
    if (terminal_pane == NULL) return;

    if (gtk_widget_get_visible(terminal_pane)) {
        gtk_widget_hide(terminal_pane);
        return;
    }

    // Shell died while hidden/visible (Ctrl+D): start a fresh one
    if (terminal_dead) {
        vte_terminal_reset(VTE_TERMINAL(terminal_widget), TRUE, TRUE);
        spawn_shell();
    }

    gtk_widget_show(terminal_pane);
    gtk_widget_grab_focus(terminal_widget);
}


// ---------------------------------------------------------------------
// Menu helpers
// ---------------------------------------------------------------------

// Adds a submenu titled `title` to the menu bar and returns it
static GtkWidget *
create_menu(GtkWidget *menu_bar, const gchar *title) {
    GtkWidget *menu = gtk_menu_new();
    GtkWidget *item = gtk_menu_item_new_with_label(title);

    gtk_menu_item_set_submenu(GTK_MENU_ITEM(item), menu);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu_bar), item);
    return menu;
}

// Adds an item with a keyboard shortcut. "<Primary>" is Cmd on macOS, Ctrl elsewhere.
static void
add_item(GtkWidget *menu, const gchar *label, const gchar *accel,
         GtkAccelGroup *group, GCallback callback, gpointer data) {
    GtkWidget *item = gtk_menu_item_new_with_label(label);

    if (accel != NULL) {
        guint key;
        GdkModifierType mods;
        gtk_accelerator_parse(accel, &key, &mods);
        if (key != 0) {
            gtk_widget_add_accelerator(item, "activate", group, key, mods, GTK_ACCEL_VISIBLE);
        }
    }

    g_signal_connect(item, "activate", callback, data);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
}

static void
add_separator(GtkWidget *menu) {
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), gtk_separator_menu_item_new());
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
    gboolean show_sidebar = FALSE; // only shown by default when a directory is opened

    // Initialize GTK
    gtk_init(&argc, &argv);
    apply_theme();

    // Resolve command-line argument: `photon file.txt` or `photon some_dir/`
    if (argc > 1) {
        gchar *arg_path = g_canonicalize_filename(argv[1], NULL);

        if (g_file_test(arg_path, G_FILE_TEST_IS_DIR)) {
            sidebar_root = arg_path; // ownership transferred, freed later
            show_sidebar = TRUE;
        } else {
            // Existing file OR a not-yet-created file either way, root the
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

    // Icon, looked up by name in the icon theme (see README) no hardcoded paths
    gtk_window_set_icon_name(GTK_WINDOW(window), "photon");

    // Editor pane
    scrolled_window = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

    // No language set at startup it's picked per-file via apply_language()
    source_buffer = gtk_source_buffer_new(NULL);
    g_source_buffer = source_buffer; // expose to sidebar callbacks

    // Apply the VS Code-style dark scheme
    scheme_manager = gtk_source_style_scheme_manager_get_default();
    scheme = gtk_source_style_scheme_manager_get_scheme(scheme_manager, "vscode-dark");
    if (scheme != NULL) {
        gtk_source_buffer_set_style_scheme(source_buffer, scheme);
    } else {
        g_printerr("Style scheme 'vscode-dark' not found (run `make install-lang`).\n");
    }

    source_view = gtk_source_view_new_with_buffer(source_buffer);
    gtk_container_add(GTK_CONTAINER(scrolled_window), source_view);
    gtk_source_view_set_show_line_numbers(GTK_SOURCE_VIEW(source_view), TRUE);

    // Menu bar + keyboard shortcuts
    GtkAccelGroup *accel = gtk_accel_group_new();
    gtk_window_add_accel_group(GTK_WINDOW(window), accel);

    GtkWidget *menu_bar   = gtk_menu_bar_new();
    GtkWidget *file_menu  = create_menu(menu_bar, "File");
    GtkWidget *edit_menu  = create_menu(menu_bar, "Edit");
    GtkWidget *view_menu  = create_menu(menu_bar, "View");

    add_item(file_menu, "Open",       "<Primary>o",         accel, G_CALLBACK(on_open_clicked),    source_buffer);
    add_item(file_menu, "Save",       "<Primary>s",         accel, G_CALLBACK(on_save_clicked),    source_buffer);
    add_item(file_menu, "Save As...", "<Primary><Shift>s",  accel, G_CALLBACK(on_save_as_clicked), source_buffer);
    add_separator(file_menu);

    GtkWidget *autosave_item = gtk_check_menu_item_new_with_label("Auto Save (every 30s)");
    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(autosave_item), FALSE); // off by default
    g_signal_connect(autosave_item, "toggled", G_CALLBACK(on_autosave_toggled), source_buffer);
    gtk_menu_shell_append(GTK_MENU_SHELL(file_menu), autosave_item);

    add_separator(file_menu);
    add_item(file_menu, "Quit",       "<Primary>q",         accel, G_CALLBACK(on_quit_clicked),    NULL);

    add_item(edit_menu, "Undo",       "<Primary>z",         accel, G_CALLBACK(on_undo_clicked),    source_buffer);
    add_item(edit_menu, "Redo",       "<Primary><Shift>z",  accel, G_CALLBACK(on_redo_clicked),    source_buffer);
    add_separator(edit_menu);
    add_item(edit_menu, "Cut",        "<Primary>x",         accel, G_CALLBACK(on_cut_clicked),        source_view);
    add_item(edit_menu, "Copy",       "<Primary>c",         accel, G_CALLBACK(on_copy_clicked),       source_view);
    add_item(edit_menu, "Paste",      "<Primary>v",         accel, G_CALLBACK(on_paste_clicked),      source_view);
    add_separator(edit_menu);
    add_item(edit_menu, "Select All", "<Primary>a",         accel, G_CALLBACK(on_select_all_clicked), source_view);

    add_item(view_menu, "Toggle Sidebar",  "<Primary>b",     accel, G_CALLBACK(on_toggle_sidebar_clicked),  NULL);
    add_item(view_menu, "Toggle Terminal", "<Primary>t", accel, G_CALLBACK(on_toggle_terminal_clicked), NULL);

    g_object_unref(accel); // the window holds its own reference

    // vbox holds the menu bar + editor/terminal split, and becomes the RIGHT pane of the split
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_box_pack_start(GTK_BOX(vbox), menu_bar, FALSE, FALSE, 0);

    // Keep a copy of sidebar_root before it's freed below the terminal needs
    // a working directory too, and build_sidebar() takes ownership-by-freeing.
    gchar *sidebar_root_backup = g_strdup(sidebar_root);

    GtkWidget *vpaned = gtk_paned_new(GTK_ORIENTATION_VERTICAL);
    gtk_paned_pack1(GTK_PANED(vpaned), scrolled_window, TRUE, FALSE);

    terminal_pane = build_terminal(sidebar_root_backup);
    g_free(sidebar_root_backup);

    gtk_paned_pack2(GTK_PANED(vpaned), terminal_pane, FALSE, FALSE);
    gtk_paned_set_position(GTK_PANED(vpaned), 450);

    gtk_box_pack_start(GTK_BOX(vbox), vpaned, TRUE, TRUE, 0);

    // Sidebar (left pane) built ONCE, rooted at sidebar_root
    // (which is either the given directory, the parent of a given file, or cwd)
    sidebar = build_sidebar(sidebar_root);
    g_free(sidebar_root);

    // Split: sidebar on the left, editor+terminal (vbox) on the right
    GtkWidget *hpaned = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_paned_pack1(GTK_PANED(hpaned), sidebar, FALSE, FALSE);
    gtk_paned_pack2(GTK_PANED(hpaned), vbox, TRUE, FALSE);
    gtk_paned_set_position(GTK_PANED(hpaned), 220);

    gtk_container_add(GTK_CONTAINER(window), hpaned);

    // Load the requested file (if any) this sets the title itself.
    // Only fall back to the default "Photon" title if nothing was requested.
    if (file_to_open != NULL) {
        if (g_file_test(file_to_open, G_FILE_TEST_EXISTS)) {
            load_file(source_buffer, file_to_open);
        } else {
            // New file: not on disk yet, but set it as current_filename so
            // Save (not Save As) writes here directly
            current_filename = g_strdup(file_to_open);
            apply_language(source_buffer, current_filename);
            update_title(current_filename);
        }
        g_free(file_to_open);
    } else {
        update_title(NULL);
    }

    // Show all widgets, then hide the sidebar/terminal unless requested
    gtk_widget_show_all(window);
    if (!show_sidebar) {
        gtk_widget_hide(sidebar);
    }
    gtk_widget_hide(terminal_pane); // terminal starts hidden, toggled via Cmd+` / Ctrl+`

    // Start the GTK main loop
    gtk_main();

    return 0;
}