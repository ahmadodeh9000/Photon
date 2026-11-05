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
 * main: startup. Parses the command line, finds the data folder (lang/,
 * styles/, icons/), sets up languages + theme, then hands off to app.c.
 *
 *   photon                 open in the current directory
 *   photon file.lunar      open a file (it may not exist yet)
 *   photon some/folder     open a folder with the sidebar shown
 *   photon --version
 *   photon --smoke-test    start, check that resources load, quit (for CI)
 *   photon --screenshot out.png [FILE]   smoke test + save the window as a PNG
 */
#include <gtk/gtk.h>

#include "app.h"
#include "languages.h"
#include "platform.h"
#include "theme.h"

#ifndef PHOTON_VERSION
#define PHOTON_VERSION "dev"
#endif

typedef struct {
    gboolean version;
    gboolean smoke_test;
    gchar   *screenshot;  // --screenshot FILE (implies --smoke-test)
    gchar   *path;        // first non-option argument, canonicalized
} Args;

static void
print_usage(void) {
    g_print("Usage: photon [--version] [--smoke-test] [--screenshot OUT.png] [FILE | FOLDER]\n");
}

// Returns FALSE on an unknown option
static gboolean
parse_args(int argc, char *argv[], Args *args) {
    for (int i = 1; i < argc; i++) {
        if (g_strcmp0(argv[i], "--version") == 0) {
            args->version = TRUE;
        } else if (g_strcmp0(argv[i], "--smoke-test") == 0) {
            args->smoke_test = TRUE;
        } else if (g_strcmp0(argv[i], "--screenshot") == 0 && i + 1 < argc) {
            args->smoke_test = TRUE;
            args->screenshot = g_strdup(argv[++i]);
        } else if (g_strcmp0(argv[i], "--help") == 0 || g_strcmp0(argv[i], "-h") == 0) {
            return FALSE;
        } else if (argv[i][0] == '-' && argv[i][1] != '\0') {
            g_printerr("Unknown option: %s\n", argv[i]);
            return FALSE;
        } else if (args->path == NULL) {
            args->path = g_canonicalize_filename(argv[i], NULL);
        }
    }
    return TRUE;
}

// Folder -> sidebar root (shown). File -> its parent folder (hidden sidebar).
static void
resolve_options(const Args *args, PhotonAppOptions *options, gchar **root_out) {
    if (args->path == NULL) {
        *root_out = g_get_current_dir();
    } else if (g_file_test(args->path, G_FILE_TEST_IS_DIR)) {
        *root_out = g_strdup(args->path);
        options->show_sidebar = TRUE;
    } else {
        *root_out = g_path_get_dirname(args->path);
        options->file = args->path;
    }
    options->root_dir = *root_out;
}

// --smoke-test: fail if the Lunar language or the style scheme didn't load
static gboolean
resources_ok(void) {
    gboolean ok = TRUE;
    if (photon_languages_get("lunar") == NULL) {
        g_printerr("smoke test: language 'lunar' not found\n");
        ok = FALSE;
    }
    if (photon_theme_get_scheme() == NULL) {
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

int
main(int argc, char *argv[]) {
    photon_platform_init();

    Args args = { 0 };
    if (!parse_args(argc, argv, &args)) {
        print_usage();
        return 2;
    }
    if (args.version) {
        g_print("Photon %s\n", PHOTON_VERSION);
        return 0;
    }

    gtk_init(&argc, &argv);

    gchar *data_dir = photon_platform_data_dir();
    if (data_dir == NULL) g_printerr("Photon data folder (lang/, styles/) not found; set PHOTON_DATA_DIR.\n");
    photon_languages_init(data_dir);
    photon_theme_init(data_dir);

    PhotonAppOptions options = { 0 };
    gchar *root_dir = NULL;
    resolve_options(&args, &options, &root_dir);

    PhotonApp *app = photon_app_new(&options);

    int status = 0;
    SmokeTest test = { app, args.screenshot };
    if (args.smoke_test) {
        status = resources_ok() ? 0 : 1;
        g_timeout_add(1500, finish_smoke_test, &test);
    }
    gtk_main();

    photon_app_free(app);
    g_free(root_dir);
    g_free(args.path);
    g_free(args.screenshot);
    g_free(data_dir);
    if (args.smoke_test && status == 0) g_print("smoke test: ok\n");
    return status;
}
