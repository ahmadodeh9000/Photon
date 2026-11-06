/**
 * @author: Ahmad Odeh
 * @date: 27th Sep, 2026
 *
**/
/*
I LOVE PHOTON LOL
MADE WITH LOVE
*/
/*
 * main: startup. Parses the command line (cli.c), then either
 *   - runs the launcher (plain `photon ...`): hand the files to a running
 *     Photon or start a detached one, and return at once, or
 *   - runs the window here (GUI mode: --detached, --foreground, or the
 *     test flags): find the data folder (lang/, styles/, icons/), set up
 *     languages + theme, hand off to app.c.
 *
 *   photon [FILE...]       open files (they may not exist yet)
 *   photon some/folder     open a folder with the sidebar shown
 *   photon -w FILE         wait until FILE's tab is closed
 *   photon --run FILE      open FILE and run it right away
 *   photon --theme ID      use a theme for this run (turbo90s, notepad95, amber,
 *                          green, hotdog, vscode-dark); View > Theme saves one
 *   photon --foreground    keep the window attached to this terminal
 *   photon --version / --help / --status
 *   photon --smoke-test    start, check that resources load, quit (for CI)
 *   photon --screenshot out.png [FILE]   smoke test + save the window as a PNG
 */
#include <gtk/gtk.h>

#include "app.h"
#include "cli.h"
#include "languages.h"
#include "platform.h"
#include "theme.h"

#ifndef PHOTON_VERSION
#define PHOTON_VERSION "dev"
#endif

// Folder -> sidebar root (shown). Files -> the first file's folder (hidden sidebar).
static gchar *
resolve_root(const PhotonArgs *args, PhotonAppOptions *options, GPtrArray *files) {
    for (guint i = 0; i < args->paths->len; i++) {
        const gchar *path = g_ptr_array_index(args->paths, i);
        if (g_file_test(path, G_FILE_TEST_IS_DIR)) {
            if (options->show_sidebar) continue;   // one folder per window
            options->show_sidebar = TRUE;
            options->root_dir = path;
        } else {
            g_ptr_array_add(files, (gpointer)path);
        }
    }
    g_ptr_array_add(files, NULL);
    options->files = (const gchar *const *)files->pdata;

    if (options->root_dir != NULL) return g_strdup(options->root_dir);
    gchar *root = files->len > 1 ? g_path_get_dirname(g_ptr_array_index(files, 0)) : g_get_current_dir();
    options->root_dir = root;
    return root;
}

// --smoke-test: fail if the Lunar language or the style scheme didn't load
static gboolean
resources_ok(PhotonTheme *theme) {
    gboolean ok = TRUE;
    if (photon_languages_get("lunar") == NULL) {
        g_printerr("smoke test: language 'lunar' not found\n");
        ok = FALSE;
    }
    if (photon_theme_get_scheme(theme) == NULL) {
        g_printerr("smoke test: style scheme not found\n");
        ok = FALSE;
    }
    return ok;
}

// Render the whole window into a PNG (no screen-capture permission needed)
static void
save_screenshot(GtkWidget *window, const gchar *png_path) {
    int width = gtk_widget_get_allocated_width(window);
    int height = gtk_widget_get_allocated_height(window);
    cairo_surface_t *surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);
    cairo_t *cr = cairo_create(surface);
    gtk_widget_draw(window, cr);
    cairo_destroy(cr);

    if (cairo_surface_write_to_png(surface, png_path) != CAIRO_STATUS_SUCCESS) {
        g_printerr("Could not write screenshot to %s\n", png_path);
    }
    cairo_surface_destroy(surface);
}

static gboolean
run_soon(gpointer user_data) {
    PhotonApp *app = user_data;
    photon_runner_run(app->runner);
    return G_SOURCE_REMOVE;
}

typedef struct {
    PhotonApp   *app;
    const gchar *screenshot;
} SmokeTest;

static gboolean
finish_smoke_test(gpointer user_data) {
    SmokeTest *test = user_data;
    if (test->screenshot != NULL) save_screenshot(test->app->window, test->screenshot);
    gtk_main_quit();
    return G_SOURCE_REMOVE;
}

// The window, in this process
static int
run_gui(int argc, char *argv[], const PhotonArgs *args) {
    if (args->detached) {
        gchar *log = photon_cli_log_path();
        photon_platform_redirect_output(log);   // nothing to the terminal
        g_free(log);
    }
    gtk_init(&argc, &argv);

    gchar *data_dir = photon_platform_data_dir();
    if (data_dir == NULL) g_printerr("Photon data folder (lang/, styles/) not found; set PHOTON_DATA_DIR.\n");
    photon_languages_init(data_dir);
    PhotonTheme *theme = photon_theme_new(data_dir, NULL);   // the app applies the saved theme

    // Smoke tests and screenshots must not touch the user's settings or the
    // running Photon: no session, read-only settings, no single-instance socket
    PhotonAppOptions options = {
        .theme = theme, .theme_id = args->theme,
        .use_session = !args->smoke_test, .read_only = args->smoke_test, .serve_remote = !args->smoke_test,
    };
    GPtrArray *files = g_ptr_array_new();
    gchar *root_dir = resolve_root(args, &options, files);

    PhotonApp *app = photon_app_new(&options);

    int status = 0;
    SmokeTest test = { app, args->screenshot };
    if (args->run && files->len > 1) g_idle_add(run_soon, app);
    if (args->smoke_test) {
        status = resources_ok(theme) ? 0 : 1;
        // A run (e.g. compiling C) gets a little longer to finish first
        g_timeout_add(args->run ? 3000 : 1500, finish_smoke_test, &test);
    }
    gtk_main();

    photon_app_free(app);
    photon_theme_free(theme);
    g_ptr_array_free(files, TRUE);
    g_free(root_dir);
    g_free(data_dir);
    if (args->smoke_test && status == 0) g_print("smoke test: ok\n");
    return status;
}

int
main(int argc, char *argv[]) {
    photon_platform_init();

    PhotonArgs args = { 0 };
    GError *error = NULL;
    int status;

    if (!photon_args_parse(argc, argv, &args, &error)) {
        g_printerr("photon: %s\n", error->message);
        g_error_free(error);
        photon_cli_print_usage();
        status = 2;
    } else if (args.help) {
        photon_cli_print_usage();
        status = 0;
    } else if (args.version) {
        g_print("Photon %s\n", PHOTON_VERSION);
        status = 0;
    } else if (photon_args_wants_gui(&args)) {
        status = run_gui(argc, argv, &args);
    } else {
        status = photon_cli_run(&args);   // the launcher: returns at once
    }

    photon_args_clear(&args);
    return status;
}
