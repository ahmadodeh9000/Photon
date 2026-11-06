/**
 * @author: Ahmad Odeh
 * @date: 6th Nov, 2026
 *
**/
/*
 * cli: command line + the launcher front-end. See cli.h.
 */
#include "cli.h"
#include "platform.h"
#include "remote.h"

#include <stdio.h>

#ifndef PHOTON_VERSION
#define PHOTON_VERSION "dev"
#endif

// How long `photon -w` waits for a freshly started Photon to answer
#define STARTUP_SECONDS 30.0

void
photon_cli_print_usage(void) {
    gchar *log = photon_cli_log_path();
    g_print(
        "Usage: photon [options] [FILE... | FOLDER]\n"
        "\n"
        "Opens the files in the running Photon (or starts one) and returns at once.\n"
        "\n"
        "  -w, --wait           wait until the files' tabs are closed (for git's core.editor)\n"
        "      --run            run the file once it's open\n"
        "      --theme ID       turbo90s, notepad95, amber, green, hotdog or vscode-dark\n"
        "      --status         list the files open in the running Photon\n"
        "      --foreground     run the window in this terminal (output shows here)\n"
        "      --version        print the version\n"
        "  -h, --help           show this help\n"
        "\n"
        "A Photon started from here keeps its output in %s\n", log);
    g_free(log);
}

gchar *
photon_cli_log_path(void) {
    return g_build_filename(g_get_user_cache_dir(), "photon", "photon.log", NULL);
}

// ---------------------------------------------------------------------
// Parsing
// ---------------------------------------------------------------------
static gboolean
take_value(int argc, char **argv, int *i, gchar **out, GError **error) {
    if (*i + 1 >= argc) {
        g_set_error(error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE, "%s needs a value", argv[*i]);
        return FALSE;
    }
    g_free(*out);
    *out = g_strdup(argv[++*i]);
    return TRUE;
}

gboolean
photon_args_parse(int argc, char **argv, PhotonArgs *args, GError **error) {
    args->paths = g_ptr_array_new_with_free_func(g_free);
    gboolean options_done = FALSE;   // after "--" everything is a path

    for (int i = 1; i < argc; i++) {
        const gchar *a = argv[i];
        if (options_done || a[0] != '-' || a[1] == '\0') {
            // Relative paths are made absolute against this process's folder
            g_ptr_array_add(args->paths, g_canonicalize_filename(a, NULL));
        } else if (g_str_equal(a, "--")) {
            options_done = TRUE;
        } else if (g_str_equal(a, "-h") || g_str_equal(a, "--help")) {
            args->help = TRUE;
        } else if (g_str_equal(a, "--version")) {
            args->version = TRUE;
        } else if (g_str_equal(a, "--status")) {
            args->status = TRUE;
        } else if (g_str_equal(a, "-w") || g_str_equal(a, "--wait")) {
            args->wait = TRUE;
        } else if (g_str_equal(a, "--run")) {
            args->run = TRUE;
        } else if (g_str_equal(a, "--foreground")) {
            args->foreground = TRUE;
        } else if (g_str_equal(a, "--detached")) {
            args->detached = TRUE;
        } else if (g_str_equal(a, "--smoke-test")) {
            args->smoke_test = TRUE;
        } else if (g_str_equal(a, "--screenshot")) {
            if (!take_value(argc, argv, &i, &args->screenshot, error)) return FALSE;
            args->smoke_test = TRUE;
        } else if (g_str_equal(a, "--theme")) {
            if (!take_value(argc, argv, &i, &args->theme, error)) return FALSE;
        } else {
            g_set_error(error, G_OPTION_ERROR, G_OPTION_ERROR_UNKNOWN_OPTION, "unknown option %s", a);
            return FALSE;
        }
    }
    return TRUE;
}

void
photon_args_clear(PhotonArgs *args) {
    if (args->paths != NULL) g_ptr_array_unref(args->paths);
    g_free(args->screenshot);
    g_free(args->theme);
    *args = (PhotonArgs){ 0 };
}

gboolean
photon_args_wants_gui(const PhotonArgs *args) {
    return args->foreground || args->detached || args->smoke_test;
}

// ---------------------------------------------------------------------
// The launcher
// ---------------------------------------------------------------------
static gboolean
has_folder(const PhotonArgs *args) {
    for (guint i = 0; i < args->paths->len; i++) {
        if (g_file_test(g_ptr_array_index(args->paths, i), G_FILE_TEST_IS_DIR)) return TRUE;
    }
    return FALSE;
}

// Hand the files to a running Photon. With `wait`, ask to hear when they close.
static gboolean
send_files(GSocketConnection *connection, const PhotonArgs *args) {
    if (args->paths->len == 0) return photon_remote_send(connection, "present", NULL);

    for (guint i = 0; i < args->paths->len; i++) {
        const gchar *verb = args->wait ? "wait" : (args->run && i == 0) ? "run" : "open";
        if (!photon_remote_send(connection, verb, g_ptr_array_index(args->paths, i))) return FALSE;
    }
    return TRUE;
}

// photon -w: block until every file has been closed (or Photon quit)
static int
wait_for_close(GSocketConnection *connection, const PhotonArgs *args) {
    guint closed = 0;
    gchar *line;
    while (closed < args->paths->len && (line = photon_remote_read_line(connection)) != NULL) {
        if (g_str_has_prefix(line, "closed ")) closed++;
        g_free(line);
    }
    return 0;
}

static int
print_status(void) {
    GSocketConnection *connection = photon_remote_connect();
    if (connection == NULL) {
        g_print("Photon is not running.\n");
        return 1;
    }
    photon_remote_send(connection, "list", NULL);
    gchar *line;
    while ((line = photon_remote_read_line(connection)) != NULL && !g_str_equal(line, "end")) {
        if (g_str_has_prefix(line, "file ")) g_print("%s\n", line + 5);
        g_free(line);
    }
    g_free(line);
    g_object_unref(connection);
    return 0;
}

// argv for the new, detached Photon
static gchar **
detached_args(const PhotonArgs *args) {
    GPtrArray *argv = g_ptr_array_new();
    g_ptr_array_add(argv, g_strdup("--detached"));
    if (args->run) g_ptr_array_add(argv, g_strdup("--run"));
    if (args->theme != NULL) {
        g_ptr_array_add(argv, g_strdup("--theme"));
        g_ptr_array_add(argv, g_strdup(args->theme));
    }
    g_ptr_array_add(argv, g_strdup("--"));
    for (guint i = 0; i < args->paths->len; i++) g_ptr_array_add(argv, g_strdup(g_ptr_array_index(args->paths, i)));
    g_ptr_array_add(argv, NULL);
    return (gchar **)g_ptr_array_free(argv, FALSE);
}

int
photon_cli_run(const PhotonArgs *args) {
    if (args->status) return print_status();
    if (args->wait && args->paths->len == 0) {
        g_printerr("photon: -w needs a file to wait for\n");
        return 2;
    }

    // A folder gets its own window; files go to the Photon that's running
    GSocketConnection *connection = has_folder(args) ? NULL : photon_remote_connect();

    if (connection == NULL) {
        gchar **argv = detached_args(args);
        GError *error = NULL;
        gboolean started = photon_platform_start_detached(argv, &error);
        g_strfreev(argv);
        if (!started) {
            g_printerr("photon: couldn't start Photon: %s\n", error->message);
            g_error_free(error);
            return 1;
        }
        if (!args->wait) return 0;

        // -w: the new Photon opens the files itself; wait until it answers
        connection = photon_remote_connect_wait(STARTUP_SECONDS);
        if (connection == NULL) {
            gchar *log = photon_cli_log_path();
            g_printerr("photon: Photon didn't start (see %s)\n", log);
            g_free(log);
            return 1;
        }
    }

    int status = 0;
    if (!send_files(connection, args)) {
        g_printerr("photon: couldn't reach the running Photon\n");
        status = 1;
    } else if (args->wait) {
        status = wait_for_close(connection, args);
    }
    g_object_unref(connection);
    return status;
}
