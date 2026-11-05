/**
 * @author: Ahmad Odeh
 * @date: 5th Nov, 2026
 *
**/
/*
 * theme: CSS for the chrome, the code style scheme, font zoom. See theme.h.
 */
#include "theme.h"

#define PHOTON_SCHEME_ID "vscode-dark"

struct PhotonTheme {
    GtkCssProvider *chrome;     // THEME_CSS
    GtkCssProvider *font_size;  // "textview { font-size: Npt }", above chrome
    gint            points;
};

// Dark chrome so the menu bar / sidebar / dialogs match the editor
static const gchar *THEME_CSS =
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

    "notebook > header { background-color: #252526; border: none; }"
    "notebook > header tab { color: #969696; padding: 2px 12px; }"
    "notebook > header tab:checked { color: #ffffff; background-color: #1e1e1e; }"

    ".photon-statusbar { background-color: #007acc; color: #ffffff; padding: 2px 10px; }"
    ".photon-statusbar label.error { color: #ffffff; font-weight: bold; }"

    // Monospace fallback chain: Menlo exists on every Mac, DejaVu Sans Mono /
    // Liberation Mono on nearly every Linux desktop.
    "textview {"
    "  font-family: 'JetBrains Mono', 'Fira Code', 'Cascadia Code',"
    "               'Source Code Pro', Menlo, 'DejaVu Sans Mono',"
    "               'Liberation Mono', monospace;"
    "}";

static void
add_search_paths(const gchar *data_dir) {
    if (data_dir == NULL) return;

    gchar *styles = g_build_filename(data_dir, "styles", NULL);
    gtk_source_style_scheme_manager_prepend_search_path(
        gtk_source_style_scheme_manager_get_default(), styles);
    g_free(styles);

    // icons/photon.png -> window icon "photon" even when not installed
    gchar *icons = g_build_filename(data_dir, "icons", NULL);
    gtk_icon_theme_append_search_path(gtk_icon_theme_get_default(), icons);
    g_free(icons);
}

static GtkCssProvider *
add_provider(guint priority) {
    GtkCssProvider *provider = gtk_css_provider_new();
    gtk_style_context_add_provider_for_screen(gdk_screen_get_default(), GTK_STYLE_PROVIDER(provider), priority);
    return provider;
}

PhotonTheme *
photon_theme_new(const gchar *data_dir) {
    PhotonTheme *theme = g_new0(PhotonTheme, 1);
    add_search_paths(data_dir);
    g_object_set(gtk_settings_get_default(), "gtk-application-prefer-dark-theme", TRUE, NULL);

    GError *error = NULL;
    theme->chrome = add_provider(GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    if (!gtk_css_provider_load_from_data(theme->chrome, THEME_CSS, -1, &error)) {
        g_printerr("Theme CSS failed to load: %s\n", error->message);
        g_error_free(error);
    }

    theme->font_size = add_provider(GTK_STYLE_PROVIDER_PRIORITY_APPLICATION + 1);
    photon_theme_set_font_size(theme, PHOTON_FONT_SIZE_DEFAULT);
    return theme;
}

void
photon_theme_free(PhotonTheme *theme) {
    if (theme == NULL) return;
    GdkScreen *screen = gdk_screen_get_default();
    gtk_style_context_remove_provider_for_screen(screen, GTK_STYLE_PROVIDER(theme->chrome));
    gtk_style_context_remove_provider_for_screen(screen, GTK_STYLE_PROVIDER(theme->font_size));
    g_object_unref(theme->chrome);
    g_object_unref(theme->font_size);
    g_free(theme);
}

GtkSourceStyleScheme *
photon_theme_get_scheme(PhotonTheme *theme G_GNUC_UNUSED) {
    return gtk_source_style_scheme_manager_get_scheme(
        gtk_source_style_scheme_manager_get_default(), PHOTON_SCHEME_ID);
}

void
photon_theme_style_buffer(PhotonTheme *theme, GtkSourceBuffer *buffer) {
    GtkSourceStyleScheme *scheme = photon_theme_get_scheme(theme);
    if (scheme == NULL) {
        g_printerr("Style scheme '%s' not found (is the styles/ folder next to Photon?).\n", PHOTON_SCHEME_ID);
        return;
    }
    gtk_source_buffer_set_style_scheme(buffer, scheme);
}

void
photon_theme_set_font_size(PhotonTheme *theme, gint points) {
    theme->points = CLAMP(points, PHOTON_FONT_SIZE_MIN, PHOTON_FONT_SIZE_MAX);
    gchar *css = g_strdup_printf("textview { font-size: %dpt; }", theme->points);
    gtk_css_provider_load_from_data(theme->font_size, css, -1, NULL);
    g_free(css);
}

gint
photon_theme_get_font_size(PhotonTheme *theme) {
    return theme->points;
}
