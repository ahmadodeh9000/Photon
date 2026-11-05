/**
 * @author: Ahmad Odeh
 * @date: 5th Nov, 2026
 *
**/
/*
 * runner: run the current file in the "Run" tab. See runner.h.
 *
 * The command runs through a tiny POSIX sh wrapper (RUN_WRAPPER) so stdin
 * and stdout stay on the terminal while stderr is copied to a temp file
 * for the error parser.
 */
#include "runner.h"
#include "app.h"
#include "settings.h"

#include <glib/gstdio.h>
#include <signal.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include <vte/vte.h>

#define SETTINGS_GROUP  "tools"
#define KILL_GRACE_MS   1000

// $1 = stderr copy, $2 = command. Stdout stays on the tty (fd 3), stderr
// goes through tee. The exit code travels through $1.status because a
// pipeline's status is tee's, not the program's.
static const gchar *RUN_WRAPPER =
    "err=\"$1\"; cmd=\"$2\"; rm -f \"$err.status\"\n"
    "exec 3>&1\n"
    "{ ( eval \"$cmd\" ); echo $? > \"$err.status\"; } 2>&1 1>&3 3>&- | tee \"$err\" 1>&2\n"
    "code=$(cat \"$err.status\" 2>/dev/null); rm -f \"$err.status\"\n"
    "exit \"${code:-1}\"\n";

// A program a run_cmd can use as {name}
typedef struct {
    const gchar *name;           // placeholder name and settings key
    const gchar *env;            // env var that overrides it, or NULL
    const gchar *candidates[4];  // looked up in PATH, in order
    gboolean     ask;            // ask with a file chooser if not found
} Tool;

static const Tool TOOLS[] = {
    { "lunar",  "LUNAR", { "lunar", NULL },                TRUE  },
    { "python", NULL,    { "python3", "python", NULL },    FALSE },
    { "cc",     "CC",    { "cc", "gcc", "clang", NULL },   FALSE },
};

struct PhotonRunner {
    PhotonApp  *app;
    GtkWidget  *page;          // scrolled window in the panel
    GtkWidget  *terminal;      // VteTerminal for runs

    GPid        pid;           // 0 when idle
    gboolean    starting;      // spawn requested, pid not known yet
    gint64      started_us;
    guint       kill_timer;

    // About the current run
    gchar                *source;   // file that was run
    gchar                *dir;      // folder it ran in
    gchar                *err_path; // stderr copy
    const PhotonLanguage *lang;

    GHashTable *make_choice;   // folder -> GINT_TO_POINTER(1 make / 2 file)
};

// ---------------------------------------------------------------------
// Tools
// ---------------------------------------------------------------------
static const Tool *
find_tool_entry(const gchar *name) {
    for (guint i = 0; i < G_N_ELEMENTS(TOOLS); i++) {
        if (g_strcmp0(TOOLS[i].name, name) == 0) return &TOOLS[i];
    }
    return NULL;
}

static gboolean
is_executable(const gchar *path) {
    return path != NULL && g_file_test(path, G_FILE_TEST_IS_EXECUTABLE) && !g_file_test(path, G_FILE_TEST_IS_DIR);
}

// Absolute path given as-is, otherwise looked up in PATH
static gchar *
resolve_program(const gchar *name) {
    if (name == NULL || *name == '\0') return NULL;
    if (g_path_is_absolute(name)) return is_executable(name) ? g_strdup(name) : NULL;
    return g_find_program_in_path(name);
}

gchar *
photon_runner_locate_lunar(PhotonRunner *runner) {
    PhotonApp *app = runner->app;
    GtkFileChooserNative *native = gtk_file_chooser_native_new("Locate the lunar program",
        GTK_WINDOW(app->window), GTK_FILE_CHOOSER_ACTION_OPEN, "_Select", "_Cancel");

    gchar *path = NULL;
    if (gtk_native_dialog_run(GTK_NATIVE_DIALOG(native)) == GTK_RESPONSE_ACCEPT) {
        path = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(native));
    }
    g_object_unref(native);

    if (path != NULL && !is_executable(path)) {
        photon_statusbar_set_message(app->statusbar, "That file isn't an executable program.", TRUE);
        g_clear_pointer(&path, g_free);
    }
    if (path != NULL) photon_settings_set_string(app->settings, SETTINGS_GROUP, "lunar", path);
    return path;
}

// $ENV, then settings.ini, then PATH, then (if allowed) ask once
static gchar *
find_tool(PhotonRunner *runner, const Tool *tool) {
    gchar *found = NULL;

    if (tool->env != NULL) found = resolve_program(g_getenv(tool->env));

    if (found == NULL) {
        gchar *saved = photon_settings_get_string(runner->app->settings, SETTINGS_GROUP, tool->name);
        found = resolve_program(saved);
        g_free(saved);
    }

    for (int i = 0; found == NULL && tool->candidates[i] != NULL; i++) {
        found = g_find_program_in_path(tool->candidates[i]);
    }

    if (found == NULL && tool->ask) found = photon_runner_locate_lunar(runner);
    return found;
}

// ---------------------------------------------------------------------
// Building the command
// ---------------------------------------------------------------------

// Value for one {placeholder}, newly allocated, or NULL if unavailable
static gchar *
placeholder_value(PhotonRunner *runner, const gchar *key, const gchar *file) {
    if (g_strcmp0(key, "file") == 0) return g_strdup(file);
    if (g_strcmp0(key, "dir") == 0) return g_path_get_dirname(file);

    gchar *base = g_path_get_basename(file);
    gchar *dot = strrchr(base, '.');
    if (dot != NULL && dot != base) *dot = '\0';
    if (g_strcmp0(key, "name") == 0) return base;
    if (g_strcmp0(key, "exe") == 0) {
        gchar *exe_name = g_strdup_printf("photon-%s", base);
        gchar *exe = g_build_filename(g_get_tmp_dir(), exe_name, NULL);
        g_free(exe_name);
        g_free(base);
        return exe;
    }
    g_free(base);

    const Tool *tool = find_tool_entry(key);
    return tool != NULL ? find_tool(runner, tool) : NULL;
}

// Expand {placeholders} in a run_cmd, shell-quoting each value.
// On failure returns NULL and sets *missing to the placeholder name.
static gchar *
expand_command(PhotonRunner *runner, const gchar *template, const gchar *file, gchar **missing) {
    GString *out = g_string_new(NULL);
    const gchar *p = template;

    while (*p != '\0') {
        const gchar *close = (*p == '{') ? strchr(p, '}') : NULL;
        if (close == NULL) {
            g_string_append_c(out, *p++);
            continue;
        }

        gchar *key = g_strndup(p + 1, (gsize)(close - p - 1));
        gchar *value = placeholder_value(runner, key, file);
        if (value == NULL) {
            *missing = key;
            g_string_free(out, TRUE);
            return NULL;
        }
        gchar *quoted = g_shell_quote(value);
        g_string_append(out, quoted);
        g_free(quoted);
        g_free(value);
        g_free(key);
        p = close + 1;
    }
    return g_string_free(out, FALSE);
}

// C file with a Makefile next to it: ask (once per folder) whether to use make
static gboolean
wants_make(PhotonRunner *runner, const gchar *dir) {
    gchar *makefile = g_build_filename(dir, "Makefile", NULL);
    gchar *makefile_lower = g_build_filename(dir, "makefile", NULL);
    gboolean has_makefile = g_file_test(makefile, G_FILE_TEST_IS_REGULAR)
                         || g_file_test(makefile_lower, G_FILE_TEST_IS_REGULAR);
    g_free(makefile);
    g_free(makefile_lower);
    gchar *make = g_find_program_in_path("make");
    gboolean can_make = has_makefile && make != NULL;
    g_free(make);
    if (!can_make) return FALSE;

    gint choice = GPOINTER_TO_INT(g_hash_table_lookup(runner->make_choice, dir));
    if (choice == 0) {
        GtkWidget *dialog = gtk_message_dialog_new(GTK_WINDOW(runner->app->window),
            GTK_DIALOG_MODAL, GTK_MESSAGE_QUESTION, GTK_BUTTONS_NONE,
            "There's a Makefile in this folder.");
        gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dialog),
            "Run `make`, or compile and run just this file?");
        gtk_dialog_add_buttons(GTK_DIALOG(dialog), "Compile This File", 2, "Run make", 1, NULL);
        gtk_dialog_set_default_response(GTK_DIALOG(dialog), 1);
        choice = gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);
        if (choice != 1 && choice != 2) return FALSE;   // closed: compile the file
        g_hash_table_insert(runner->make_choice, g_strdup(dir), GINT_TO_POINTER(choice));
    }
    return choice == 1;
}

// ---------------------------------------------------------------------
// Finishing a run
// ---------------------------------------------------------------------
static void
feed(PhotonRunner *runner, const gchar *text) {
    vte_terminal_feed(VTE_TERMINAL(runner->terminal), text, -1);
}

static gchar *
describe_exit(gint wait_status) {
    if (WIFEXITED(wait_status)) return g_strdup_printf("Exit %d", WEXITSTATUS(wait_status));
    if (WIFSIGNALED(wait_status)) return g_strdup_printf("Stopped (signal %d)", WTERMSIG(wait_status));
    return g_strdup("Finished");
}

// Last non-empty line of the stderr copy, for errors without a location
static gchar *
last_line(const gchar *text) {
    gchar **lines = g_strsplit(text, "\n", -1);
    gchar *last = NULL;
    for (int i = 0; lines[i] != NULL; i++) {
        if (*g_strstrip(lines[i]) != '\0') {
            g_free(last);
            last = g_strdup(lines[i]);
        }
    }
    g_strfreev(lines);
    return last;
}

static void
report_errors(PhotonRunner *runner, gboolean failed) {
    gchar *output = NULL;
    g_file_get_contents(runner->err_path, &output, NULL, NULL);
    if (output == NULL) return;

    GPtrArray *errors = NULL;
    if (runner->lang != NULL && runner->lang->error_parser != NULL) {
        errors = runner->lang->error_parser(output, runner->source, runner->dir);
    }

    if (errors != NULL && errors->len > 0) {
        photon_app_show_errors(runner->app, errors);
    } else if (failed) {
        gchar *line = last_line(output);
        if (line != NULL) photon_statusbar_set_message(runner->app->statusbar, line, TRUE);
        g_free(line);
    }

    if (errors != NULL) g_ptr_array_unref(errors);
    g_free(output);
}

static void
clear_run_state(PhotonRunner *runner) {
    if (runner->kill_timer != 0) g_source_remove(runner->kill_timer);
    runner->kill_timer = 0;
    runner->pid = 0;
    runner->starting = FALSE;
    if (runner->err_path != NULL) g_unlink(runner->err_path);
    g_clear_pointer(&runner->err_path, g_free);
    g_clear_pointer(&runner->source, g_free);
    g_clear_pointer(&runner->dir, g_free);
    runner->lang = NULL;
}

static void
on_run_exited(VteTerminal *term G_GNUC_UNUSED, gint wait_status, gpointer user_data) {
    PhotonRunner *runner = user_data;
    gdouble seconds = (g_get_monotonic_time() - runner->started_us) / 1e6;
    gboolean ok = WIFEXITED(wait_status) && WEXITSTATUS(wait_status) == 0;

    gchar *what = describe_exit(wait_status);
    gchar *status = g_strdup_printf("%s · %.2f s", what, seconds);
    gchar *footer = g_strdup_printf("\r\n\x1b[%sm[%s]\x1b[0m\r\n", ok ? "32" : "31", status);
    feed(runner, footer);
    photon_statusbar_set_run_status(runner->app->statusbar, status);

    report_errors(runner, !ok);
    clear_run_state(runner);
    g_free(what);
    g_free(status);
    g_free(footer);
}

// ---------------------------------------------------------------------
// Starting a run
// ---------------------------------------------------------------------
static void
on_spawned(VteTerminal *term G_GNUC_UNUSED, GPid pid, GError *error, gpointer user_data) {
    PhotonRunner *runner = user_data;
    runner->starting = FALSE;

    if (error != NULL) {
        photon_statusbar_set_message(runner->app->statusbar, error->message, TRUE);
        photon_statusbar_set_run_status(runner->app->statusbar, "");
        clear_run_state(runner);
        return;
    }
    runner->pid = pid;
}

static void
spawn(PhotonRunner *runner, const gchar *command) {
    gchar *header = g_strdup_printf("\x1b[1m$ %s\x1b[0m\r\n", command);
    vte_terminal_reset(VTE_TERMINAL(runner->terminal), TRUE, TRUE);
    feed(runner, header);
    g_free(header);

    gchar *argv[] = { "/bin/sh", "-c", (gchar *)RUN_WRAPPER, "photon-run", runner->err_path, (gchar *)command, NULL };
    runner->starting = TRUE;
    runner->started_us = g_get_monotonic_time();
    vte_terminal_spawn_async(VTE_TERMINAL(runner->terminal), VTE_PTY_DEFAULT, runner->dir, argv, NULL,
        G_SPAWN_DEFAULT, NULL, NULL, NULL, -1, NULL, on_spawned, runner);

    photon_panel_show_tab(runner->app->panel, runner->page);
    gtk_widget_grab_focus(runner->terminal);
    photon_statusbar_set_run_status(runner->app->statusbar, "Running…");
}

static gchar *
new_err_file(void) {
    GError *error = NULL;
    gchar *path = NULL;
    gint fd = g_file_open_tmp("photon-run-XXXXXX.err", &path, &error);
    if (fd < 0) {
        g_printerr("Could not create a temp file: %s\n", error->message);
        g_error_free(error);
        return NULL;
    }
    close(fd);
    return path;
}

// The command for this run, or NULL (with a status message) if it can't run
static gchar *
build_command(PhotonRunner *runner, const PhotonLanguage *lang, const gchar *file, const gchar *dir) {
    PhotonStatusbar *bar = runner->app->statusbar;

    if (g_strcmp0(lang->id, "c") == 0 && wants_make(runner, dir)) return g_strdup("make");

    gchar *missing = NULL;
    gchar *command = expand_command(runner, lang->run_cmd, file, &missing);
    if (command == NULL) {
        gchar *msg = g_strdup_printf("Can't run %s: '%s' not found (install it or set its path).", lang->name, missing);
        photon_statusbar_set_message(bar, msg, TRUE);
        g_free(msg);
        g_free(missing);
    }
    return command;
}

void
photon_runner_run(PhotonRunner *runner) {
    PhotonApp *app = runner->app;

    if (photon_runner_is_running(runner)) {
        photon_statusbar_set_message(app->statusbar, "Already running. Stop it first.", TRUE);
        return;
    }
    if (!photon_app_save(app, photon_app_current_doc(app))) return;   // cancelled or failed

    PhotonDocument *doc = photon_app_current_doc(app);
    const gchar *file = photon_document_get_path(doc);
    const PhotonLanguage *lang = photon_document_get_language(doc);
    if (lang == NULL || lang->run_cmd == NULL) {
        photon_statusbar_set_message(app->statusbar, "Photon doesn't know how to run this file.", TRUE);
        return;
    }

    gchar *dir = g_path_get_dirname(file);
    gchar *command = build_command(runner, lang, file, dir);
    gchar *err_path = command != NULL ? new_err_file() : NULL;
    if (err_path == NULL) {
        g_free(command);
        g_free(dir);
        return;
    }

    photon_app_clear_errors(app);
    photon_statusbar_set_message(app->statusbar, NULL, FALSE);
    runner->source = g_strdup(file);
    runner->dir = dir;
    runner->err_path = err_path;
    runner->lang = lang;
    spawn(runner, command);
    g_free(command);
}

// ---------------------------------------------------------------------
// Stopping
// ---------------------------------------------------------------------

// VTE starts the child in its own session, so -pid is the whole group:
// the sh wrapper, tee and the program.
static gboolean
kill_hard(gpointer user_data) {
    PhotonRunner *runner = user_data;
    runner->kill_timer = 0;
    if (runner->pid > 0) kill(-runner->pid, SIGKILL);
    return G_SOURCE_REMOVE;
}

void
photon_runner_stop(PhotonRunner *runner) {
    if (runner->pid <= 0) return;
    kill(-runner->pid, SIGTERM);
    if (runner->kill_timer == 0) runner->kill_timer = g_timeout_add(KILL_GRACE_MS, kill_hard, runner);
    photon_statusbar_set_run_status(runner->app->statusbar, "Stopping…");
}

gboolean
photon_runner_is_running(PhotonRunner *runner) {
    return runner->pid > 0 || runner->starting;
}

// ---------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------
PhotonRunner *
photon_runner_new(PhotonApp *app) {
    PhotonRunner *runner = g_new0(PhotonRunner, 1);
    runner->app = app;
    runner->make_choice = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);

    runner->terminal = vte_terminal_new();
    GdkRGBA fg, bg;
    gdk_rgba_parse(&fg, "#cccccc");
    gdk_rgba_parse(&bg, "#1e1e1e");
    vte_terminal_set_colors(VTE_TERMINAL(runner->terminal), &fg, &bg, NULL, 0);
    g_signal_connect(runner->terminal, "child-exited", G_CALLBACK(on_run_exited), runner);

    runner->page = gtk_scrolled_window_new(NULL, NULL);
    gtk_container_add(GTK_CONTAINER(runner->page), runner->terminal);
    photon_panel_add_tab(app->panel, runner->page, "Run");
    return runner;
}

void
photon_runner_free(PhotonRunner *runner) {
    if (runner == NULL) return;
    photon_runner_stop(runner);
    g_signal_handlers_disconnect_by_data(runner->terminal, runner);
    clear_run_state(runner);
    g_hash_table_destroy(runner->make_choice);
    g_free(runner);
}
