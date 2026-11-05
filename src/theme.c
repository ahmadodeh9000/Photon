/**
 * @author: Ahmad Odeh
 * @date: 5th Nov, 2026
 *
**/
/*
 * theme: CSS for the chrome + the code style scheme. See theme.h.
 */
#include "theme.h"

#define PHOTON_SCHEME_ID "vscode-dark"

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

    // Monospace fallback chain: Menlo exists on every Mac, DejaVu Sans Mono /
    // Liberation Mono on nearly every Linux desktop.
    "textview {"
    "  font-family: 'JetBrains Mono', 'Fira Code', 'Cascadia Code',"
    "               'Source Code Pro', Menlo, 'DejaVu Sans Mono',"
    "               'Liberation Mono', monospace;"
    "  font-size: 11pt;"
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

static void
apply_css(void) {
    g_object_set(gtk_settings_get_default(), "gtk-application-prefer-dark-theme", TRUE, NULL);

    GtkCssProvider *provider = gtk_css_provider_new();
    GError *error = NULL;

    if (!gtk_css_provider_load_from_data(provider, THEME_CSS, -1, &error)) {
        g_printerr("Theme CSS failed to load: %s\n", error->message);
        g_error_free(error);
    } else {
        gtk_style_context_add_provider_for_screen(gdk_screen_get_default(),
            GTK_STYLE_PROVIDER(provider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    }
    g_object_unref(provider);
}

void
photon_theme_init(const gchar *data_dir) {
    add_search_paths(data_dir);
    apply_css();
}

GtkSourceStyleScheme *
photon_theme_get_scheme(void) {
    return gtk_source_style_scheme_manager_get_scheme(
        gtk_source_style_scheme_manager_get_default(), PHOTON_SCHEME_ID);
}

void
photon_theme_style_buffer(GtkSourceBuffer *buffer) {
    GtkSourceStyleScheme *scheme = photon_theme_get_scheme();
    if (scheme == NULL) {
        g_printerr("Style scheme '%s' not found (is the styles/ folder next to Photon?).\n",
                   PHOTON_SCHEME_ID);
        return;
    }
    gtk_source_buffer_set_style_scheme(buffer, scheme);
}
