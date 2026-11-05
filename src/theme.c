/**
 * @author: Ahmad Odeh
 * @date: 5th Nov, 2026
 *
**/
/*
 * theme: the theme registry, CSS, terminals, font zoom. See theme.h.
 *
 * To add a theme: write its CSS below, add styles/<id>.xml, and add one
 * entry to THEMES.
 */
#include "theme.h"

// Font fallback chains: the first that exists wins. Every chain ends in
// fonts that ship with macOS (Menlo, Tahoma, Geneva) and with most Linux
// desktops (DejaVu, Liberation).
#define DOS_FONTS  "'Px437 IBM VGA 8x16', 'Px437 IBM VGA8', 'Perfect DOS VGA 437', " \
                   "'Perfect DOS VGA 437 Win', Menlo, 'DejaVu Sans Mono', 'Liberation Mono', monospace"
#define WIN95_FONTS "'Pixelated MS Sans Serif', 'MS Sans Serif', 'Microsoft Sans Serif', " \
                    "Tahoma, Geneva, 'DejaVu Sans', 'Liberation Sans', sans-serif"
#define MODERN_MONO "'JetBrains Mono', 'Fira Code', 'Cascadia Code', 'Source Code Pro', " \
                    "Menlo, 'DejaVu Sans Mono', 'Liberation Mono', monospace"

// Win95 bevels: two lines each side. Raised = light top-left, dark bottom-right.
#define RAISED  "border: 1px solid; border-color: #ffffff #000000 #000000 #ffffff;" \
                "box-shadow: inset -1px -1px #808080, inset 1px 1px #dfdfdf;"
#define PRESSED "border: 1px solid; border-color: #000000 #ffffff #ffffff #000000;" \
                "box-shadow: inset 1px 1px #808080, inset -1px -1px #dfdfdf;"
#define SUNKEN  "border: 2px solid; border-color: #808080 #ffffff #ffffff #808080;"

// ---------------------------------------------------------------------
// Turbo 90s: Windows 95 chrome around a Borland Turbo C++ code area
// ---------------------------------------------------------------------
static const gchar *TURBO_CSS =
    "* { border-radius: 0; -gtk-outline-radius: 0; text-shadow: none; -gtk-icon-shadow: none; }"
    "window, .background, dialog, messagedialog, popover {"
    "  background-color: #c0c0c0; color: #000000; font-family: " WIN95_FONTS "; font-size: 12px; }"
    "label { color: #000000; }"
    "label:disabled { color: #808080; text-shadow: 1px 1px #ffffff; }"

    // Menus
    "menubar { background-color: #c0c0c0; padding: 1px 2px; box-shadow: none; border-bottom: 1px solid #808080; }"
    "menubar > menuitem { padding: 2px 8px; color: #000000; }"
    "menubar > menuitem:hover, menubar > menuitem:hover > label { background-color: #000080; color: #ffffff; }"
    "menu, .menu, .context-menu { background-color: #c0c0c0; padding: 2px; " RAISED " }"
    "menu menuitem { padding: 3px 20px 3px 16px; min-height: 16px; color: #000000; }"
    "menu menuitem:hover { background-color: #000080; color: #ffffff; }"
    "menu menuitem:hover label, menu menuitem:hover accelerator { color: #ffffff; }"
    "menu menuitem accelerator { color: #000000; }"
    "menu menuitem:disabled label { color: #808080; }"
    "menu separator { background: none; min-height: 0; margin: 3px 2px;"
    "  border-top: 1px solid #808080; border-bottom: 1px solid #ffffff; }"
    "menu menuitem check, menu menuitem radio { color: #000000; background: none; border: none; box-shadow: none; }"
    "menu menuitem:hover check, menu menuitem:hover radio { color: #ffffff; }"

    // Buttons
    "button { background-color: #c0c0c0; background-image: none; color: #000000;"
    "  padding: 3px 10px; min-height: 18px; " RAISED " }"
    "button:hover { background-color: #c0c0c0; }"
    "button:active, button:checked { background-color: #c0c0c0; " PRESSED " }"
    "button:disabled { color: #808080; }"
    "button label { color: #000000; }"
    "button:focus { outline: 1px dotted #000000; outline-offset: -4px; }"

    // Text fields
    "entry { background-color: #ffffff; color: #000000; padding: 2px 4px; min-height: 18px;"
    "  border: 1px solid; border-color: #808080 #ffffff #ffffff #808080;"
    "  box-shadow: inset 1px 1px #000000, inset -1px -1px #dfdfdf; background-image: none; }"
    "entry selection, label selection { background-color: #000080; color: #ffffff; }"
    "entry image { color: #000000; }"

    // Tabs (editor + bottom panel)
    "notebook { background-color: #c0c0c0; }"
    "notebook > header { background-color: #c0c0c0; border: none; padding: 3px 3px 0 3px; box-shadow: none; }"
    "notebook > header tab { background-color: #c0c0c0; color: #000000; padding: 2px 10px; margin: 0 1px; min-height: 18px;"
    "  border: 1px solid; border-color: #ffffff #000000 #c0c0c0 #ffffff; box-shadow: inset -1px 0 #808080, inset 1px 1px #dfdfdf; }"
    "notebook > header tab label { color: #000000; }"
    "notebook > header tab:checked { background-color: #000080; }"
    "notebook > header tab:checked label { color: #ffffff; font-weight: bold; }"
    "notebook > header tab button.photon-tab-close { padding: 0 3px; min-height: 0; min-width: 0; margin: 0;"
    "  border: none; box-shadow: none; background: none; }"
    "notebook > header tab:checked button label { color: #ffffff; }"
    "notebook > header > tabs > arrow { color: #000000; }"
    "notebook > stack:not(:only-child) { background-color: #c0c0c0; }"

    // Win95 title-bar strip above the tabs
    ".photon-titlebar { background-image: linear-gradient(to right, #000080, #1084d0);"
    "  padding: 2px 6px; min-height: 18px; }"
    ".photon-titlebar label { color: #ffffff; font-weight: bold; }"

    // Sunken wells: editor, sidebar, terminals
    "scrolledwindow.photon-sunken { background-color: #c0c0c0; " SUNKEN " }"

    // Sidebar
    "treeview.view { background-color: #ffffff; color: #000000; }"
    "treeview.view:hover { background-color: #ffffff; }"
    "treeview.view:selected, treeview.view:selected:focus { background-color: #000080; color: #ffffff; }"
    "treeview.view.expander { color: #000000; }"

    // Chunky Win95 scrollbars: grey track, bevelled thumb and arrow buttons
    "scrollbar { background-color: #dfdfdf; background-image: none; border: none; padding: 0;"
    "  -GtkScrollbar-has-backward-stepper: true; -GtkScrollbar-has-forward-stepper: true; }"
    "scrollbar slider { background-color: #c0c0c0; min-width: 14px; min-height: 14px; margin: 0; " RAISED " }"
    "scrollbar.vertical slider { min-height: 24px; }"
    "scrollbar.horizontal slider { min-width: 24px; }"
    "scrollbar button { background-color: #c0c0c0; color: #000000; min-width: 14px; min-height: 14px;"
    "  padding: 0; " RAISED " }"
    "scrollbar button:active { " PRESSED " }"
    "scrollbar button:disabled { color: #808080; }"
    "scrollbar contents trough { background-color: #dfdfdf; border: none; }"
    "scrolledwindow junction { background-color: #c0c0c0; }"

    // Splitters
    "paned > separator { background-color: #c0c0c0; background-image: none; min-width: 4px; min-height: 4px; }"

    // Status bar, search bar
    ".photon-statusbar { background-color: #c0c0c0; padding: 2px 6px; " RAISED " }"
    ".photon-statusbar > label { padding: 0 6px; border: 1px solid; border-color: #808080 #ffffff #ffffff #808080; }"
    ".photon-statusbar label.error { color: #aa0000; font-weight: bold; }"
    ".photon-statusbar button { padding: 0 6px; min-height: 0; }"
    ".photon-search { background-color: #c0c0c0; border-bottom: 1px solid #808080; }"

    // Tooltips: Win95 yellow
    "tooltip, tooltip.background { background-color: #ffffe1; border: 1px solid #000000; box-shadow: none; }"
    "tooltip label { color: #000000; }"

    // Code: DOS font, Turbo C++ caret (wide and see-through)
    "textview { font-family: " DOS_FONTS "; caret-color: rgba(255, 255, 85, 0.65);"
    "  -GtkWidget-cursor-aspect-ratio: 0.55; }"

    // Completion popup in the same blue
    ".gtksourcecompletion, .gtksourcecompletion treeview { background-color: #00aaaa; color: #000000; }"
    ".gtksourcecompletion treeview:selected { background-color: #000080; color: #ffffff; }"

    // About dialog
    ".photon-about-box { background-color: #c0c0c0; padding: 12px; }"
    ".photon-about-well { background-color: #ffffff; padding: 10px; " SUNKEN " }"
    ".photon-about-well label { color: #000000; }";

// ---------------------------------------------------------------------
// VS Code Dark: the original look
// ---------------------------------------------------------------------
static const gchar *VSCODE_CSS =
    "window, .background { background-color: #1e1e1e; color: #cccccc; }"

    "menubar { background-color: #333333; color: #cccccc; padding: 0; }"
    "menubar > menuitem { color: #cccccc; padding: 4px 10px; }"
    "menubar > menuitem:hover { background-color: #094771; color: #ffffff; }"
    "menu, .menu { background-color: #252526; color: #cccccc; border: 1px solid #454545; }"
    "menuitem { color: #cccccc; }"
    "menuitem:hover { background-color: #094771; color: #ffffff; }"
    "separator { background-color: #454545; }"

    "treeview.view { background-color: #252526; color: #cccccc; }"
    "treeview.view:hover { background-color: #2a2d2e; }"
    "treeview.view:selected, treeview.view:selected:focus { background-color: #094771; color: #ffffff; }"

    "paned > separator { background-image: image(#3c3c3c); }"

    "notebook > header { background-color: #252526; border: none; }"
    "notebook > header tab { color: #969696; padding: 2px 12px; }"
    "notebook > header tab:checked { color: #ffffff; background-color: #1e1e1e; }"
    "notebook > header tab button { padding: 0 2px; min-height: 0; min-width: 0; }"

    ".photon-statusbar { background-color: #007acc; color: #ffffff; padding: 2px 10px; }"
    ".photon-statusbar label { color: #ffffff; }"
    ".photon-statusbar label.error { font-weight: bold; }"
    ".photon-statusbar button { color: #ffffff; padding: 0 6px; min-height: 0; }"
    ".photon-search { background-color: #252526; }"

    ".photon-about-box { padding: 12px; }"
    ".photon-about-well { padding: 10px; }"

    "textview { font-family: " MODERN_MONO "; }";

// ---------------------------------------------------------------------
// Registry
// ---------------------------------------------------------------------
typedef struct {
    const gchar *id;
    const gchar *name;
    const gchar *const *css;      // pointer to the CSS string above
    const gchar *scheme;          // styles/<scheme>.xml id
    gboolean     dark;            // ask GTK for its dark variant underneath
    gboolean     title_strip;     // gradient strip above the tabs
    const gchar *term_fg;
    const gchar *term_bg;
    const gchar *term_font;       // Pango list, first installed family wins
    const gchar *error_tint;      // background of lines with errors
    const gchar *warning_tint;
} ThemeInfo;

static const ThemeInfo THEMES[] = {
    {
        .id = "turbo90s", .name = "Turbo 90s",
        .css = &TURBO_CSS, .scheme = "turbo90s",
        .dark = FALSE, .title_strip = TRUE,
        .term_fg = "#aaaaaa", .term_bg = "#000000",
        .term_font = "Px437 IBM VGA 8x16, Perfect DOS VGA 437, Menlo, DejaVu Sans Mono, monospace 12",
        .error_tint = "#aa0000", .warning_tint = "#aa5500",
    },
    {
        .id = "vscode-dark", .name = "VS Code Dark",
        .css = &VSCODE_CSS, .scheme = "vscode-dark",
        .dark = TRUE, .title_strip = FALSE,
        .term_fg = "#cccccc", .term_bg = "#1e1e1e",
        .term_font = "JetBrains Mono, Fira Code, Menlo, DejaVu Sans Mono, monospace 11",
        .error_tint = "rgba(244,71,71,0.18)", .warning_tint = "rgba(215,186,125,0.15)",
    },
};

static const gchar *const THEME_IDS[] = { "turbo90s", "vscode-dark", NULL };

struct PhotonTheme {
    const ThemeInfo *info;
    GtkCssProvider  *chrome;     // the theme's CSS
    GtkCssProvider  *font_size;  // "textview { font-size: Npt }", above chrome
    gint             points;
    GSList          *buffers;    // weak: styled GtkSourceBuffers
    GSList          *terminals;  // weak: styled VteTerminals
};

static const ThemeInfo *
find_theme(const gchar *id) {
    for (guint i = 0; i < G_N_ELEMENTS(THEMES); i++) {
        if (g_strcmp0(THEMES[i].id, id) == 0) return &THEMES[i];
    }
    return NULL;
}

const gchar *const *
photon_theme_ids(void) {
    return THEME_IDS;
}

const gchar *
photon_theme_name(const gchar *id) {
    const ThemeInfo *info = find_theme(id);
    return info != NULL ? info->name : id;
}

// ---------------------------------------------------------------------
// Styling things
// ---------------------------------------------------------------------
GtkSourceStyleScheme *
photon_theme_get_scheme(PhotonTheme *theme) {
    return gtk_source_style_scheme_manager_get_scheme(
        gtk_source_style_scheme_manager_get_default(), theme->info->scheme);
}

static void
style_buffer(PhotonTheme *theme, GtkSourceBuffer *buffer) {
    GtkSourceStyleScheme *scheme = photon_theme_get_scheme(theme);
    if (scheme == NULL) {
        g_printerr("Style scheme '%s' not found (is the styles/ folder next to Photon?).\n", theme->info->scheme);
        return;
    }
    gtk_source_buffer_set_style_scheme(buffer, scheme);
}

static void
style_terminal(PhotonTheme *theme, VteTerminal *terminal) {
    GdkRGBA fg, bg;
    gdk_rgba_parse(&fg, theme->info->term_fg);
    gdk_rgba_parse(&bg, theme->info->term_bg);
    vte_terminal_set_colors(terminal, &fg, &bg, NULL, 0);
    vte_terminal_set_color_cursor(terminal, &fg);

    PangoFontDescription *font = pango_font_description_from_string(theme->info->term_font);
    vte_terminal_set_font(terminal, font);
    pango_font_description_free(font);
}

// Drop finalized objects from a weak list
static void
forget(gpointer list_ptr, GObject *dead) {
    GSList **list = list_ptr;
    *list = g_slist_remove(*list, dead);
}

void
photon_theme_attach_buffer(PhotonTheme *theme, GtkSourceBuffer *buffer) {
    style_buffer(theme, buffer);
    if (g_slist_find(theme->buffers, buffer) != NULL) return;
    theme->buffers = g_slist_prepend(theme->buffers, buffer);
    g_object_weak_ref(G_OBJECT(buffer), forget, &theme->buffers);
}

void
photon_theme_attach_terminal(PhotonTheme *theme, VteTerminal *terminal) {
    style_terminal(theme, terminal);
    if (g_slist_find(theme->terminals, terminal) != NULL) return;
    theme->terminals = g_slist_prepend(theme->terminals, terminal);
    g_object_weak_ref(G_OBJECT(terminal), forget, &theme->terminals);
}

// ---------------------------------------------------------------------
// Switching
// ---------------------------------------------------------------------
static void
add_search_paths(const gchar *data_dir) {
    if (data_dir == NULL) return;

    gchar *styles = g_build_filename(data_dir, "styles", NULL);
    gtk_source_style_scheme_manager_prepend_search_path(gtk_source_style_scheme_manager_get_default(), styles);
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

gboolean
photon_theme_set(PhotonTheme *theme, const gchar *id) {
    const ThemeInfo *info = find_theme(id);
    if (info == NULL) return FALSE;
    theme->info = info;

    g_object_set(gtk_settings_get_default(), "gtk-application-prefer-dark-theme", info->dark, NULL);
    GError *error = NULL;
    if (!gtk_css_provider_load_from_data(theme->chrome, *info->css, -1, &error)) {
        g_printerr("Theme CSS failed to load: %s\n", error->message);
        g_error_free(error);
    }

    for (GSList *l = theme->buffers; l != NULL; l = l->next) style_buffer(theme, l->data);
    for (GSList *l = theme->terminals; l != NULL; l = l->next) style_terminal(theme, l->data);
    return TRUE;
}

PhotonTheme *
photon_theme_new(const gchar *data_dir, const gchar *id) {
    PhotonTheme *theme = g_new0(PhotonTheme, 1);
    add_search_paths(data_dir);

    theme->chrome = add_provider(GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    theme->font_size = add_provider(GTK_STYLE_PROVIDER_PRIORITY_APPLICATION + 1);
    if (id == NULL || !photon_theme_set(theme, id)) photon_theme_set(theme, PHOTON_THEME_DEFAULT);
    photon_theme_set_font_size(theme, PHOTON_FONT_SIZE_DEFAULT);
    return theme;
}

void
photon_theme_free(PhotonTheme *theme) {
    if (theme == NULL) return;
    for (GSList *l = theme->buffers; l != NULL; l = l->next) g_object_weak_unref(l->data, forget, &theme->buffers);
    for (GSList *l = theme->terminals; l != NULL; l = l->next) g_object_weak_unref(l->data, forget, &theme->terminals);
    g_slist_free(theme->buffers);
    g_slist_free(theme->terminals);

    GdkScreen *screen = gdk_screen_get_default();
    gtk_style_context_remove_provider_for_screen(screen, GTK_STYLE_PROVIDER(theme->chrome));
    gtk_style_context_remove_provider_for_screen(screen, GTK_STYLE_PROVIDER(theme->font_size));
    g_object_unref(theme->chrome);
    g_object_unref(theme->font_size);
    g_free(theme);
}

const gchar *
photon_theme_get_id(PhotonTheme *theme) {
    return theme->info->id;
}

gboolean
photon_theme_has_title_strip(PhotonTheme *theme) {
    return theme->info->title_strip;
}

const gchar *
photon_theme_error_tint(PhotonTheme *theme) {
    return theme->info->error_tint;
}

const gchar *
photon_theme_warning_tint(PhotonTheme *theme) {
    return theme->info->warning_tint;
}

void
photon_theme_make_well(GtkWidget *scrolled_window) {
    gtk_scrolled_window_set_overlay_scrolling(GTK_SCROLLED_WINDOW(scrolled_window), FALSE);
    gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scrolled_window), GTK_SHADOW_IN);
    gtk_style_context_add_class(gtk_widget_get_style_context(scrolled_window), "photon-sunken");
}

// ---------------------------------------------------------------------
// Zoom
// ---------------------------------------------------------------------
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
