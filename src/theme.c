/**
 * @author: Ahmad Odeh
 * @date: 5th Nov, 2026
 *
**/
/*
 * theme: the theme registry, CSS, terminals, font zoom. See theme.h.
 *
 * Most themes share one Windows 95 CSS template (WIN95_CSS) and differ only
 * in their palette: @name@ tokens in the template are filled from the
 * theme's Var list. To add a theme: write a palette, add styles/<id>.xml,
 * and add one entry to THEMES.
 */
#include "theme.h"

#include <string.h>

// Font fallback chains: the first that exists wins. Every chain ends in
// fonts that ship with macOS (Menlo, Monaco, Tahoma, Geneva) and with most
// Linux desktops (DejaVu, Liberation).
#define DOS_FONTS  "'Px437 IBM VGA 8x16', 'Px437 IBM VGA8', 'Perfect DOS VGA 437', " \
                   "'Perfect DOS VGA 437 Win', Menlo, 'DejaVu Sans Mono', 'Liberation Mono', monospace"
#define FIXEDSYS_FONTS "'Fixedsys Excelsior 3.01', 'Fixedsys Excelsior', Fixedsys, 'Lucida Console', " \
                       "Monaco, 'DejaVu Sans Mono', 'Liberation Mono', monospace"
#define WIN95_FONTS "'Pixelated MS Sans Serif', 'MS Sans Serif', 'Microsoft Sans Serif', " \
                    "Tahoma, Geneva, 'DejaVu Sans', 'Liberation Sans', sans-serif"
#define MODERN_MONO "'JetBrains Mono', 'Fira Code', 'Cascadia Code', 'Source Code Pro', " \
                    "Menlo, 'DejaVu Sans Mono', 'Liberation Mono', monospace"

// Pango lists for the terminals (same idea, Pango syntax)
#define DOS_TERM_FONT      "Px437 IBM VGA 8x16, Perfect DOS VGA 437, Menlo, DejaVu Sans Mono, monospace 12"
#define FIXEDSYS_TERM_FONT "Fixedsys Excelsior 3.01, Lucida Console, Monaco, DejaVu Sans Mono, monospace 12"

// Win95 bevels: two lines each side. Raised = light top-left, dark bottom-right.
#define RAISED  "border: 1px solid; border-color: @hilite@ @dark@ @dark@ @hilite@;" \
                "box-shadow: inset -1px -1px @shadow@, inset 1px 1px @light@;"
#define PRESSED "border: 1px solid; border-color: @dark@ @hilite@ @hilite@ @dark@;" \
                "box-shadow: inset 1px 1px @shadow@, inset -1px -1px @light@;"
#define SUNKEN  "border: 2px solid; border-color: @shadow@ @hilite@ @hilite@ @shadow@;"

// ---------------------------------------------------------------------
// The Windows 95 template. Tokens:
//   @face@ @light@ @hilite@ @shadow@ @dark@   3D face and its bevel lines
//   @text@ @disabled@                         text on the face
//   @sel_bg@ @sel_fg@                         selection, active tab, hovered menu item
//   @title_a@ @title_b@ @title_fg@            title strip gradient
//   @well_bg@ @well_fg@                       entries, sidebar, About box
//   @track@ @tip_bg@ @tip_fg@ @error_fg@      scrollbar track, tooltips, status errors
//   @popup_bg@ @popup_fg@                     completion popup
//   @ui_font@ @code_font@ @caret@
// ---------------------------------------------------------------------
static const gchar *WIN95_CSS =
    "* { border-radius: 0; -gtk-outline-radius: 0; text-shadow: none; -gtk-icon-shadow: none; }"
    "window, .background, dialog, messagedialog, popover {"
    "  background-color: @face@; color: @text@; font-family: @ui_font@; font-size: 12px; }"
    "label { color: @text@; }"
    "label:disabled { color: @disabled@; }"

    // Menus
    "menubar { background-color: @face@; padding: 1px 2px; box-shadow: none; border-bottom: 1px solid @shadow@; }"
    "menubar > menuitem { padding: 2px 8px; color: @text@; }"
    "menubar > menuitem:hover, menubar > menuitem:hover > label { background-color: @sel_bg@; color: @sel_fg@; }"
    "menu, .menu, .context-menu { background-color: @face@; padding: 2px; " RAISED " }"
    "menu menuitem { padding: 3px 20px 3px 16px; min-height: 16px; color: @text@; }"
    "menu menuitem:hover { background-color: @sel_bg@; color: @sel_fg@; }"
    "menu menuitem:hover label, menu menuitem:hover accelerator { color: @sel_fg@; }"
    "menu menuitem accelerator { color: @text@; }"
    "menu menuitem:disabled label { color: @disabled@; }"
    "menu separator { background: none; min-height: 0; margin: 3px 2px;"
    "  border-top: 1px solid @shadow@; border-bottom: 1px solid @hilite@; }"
    "menu menuitem check, menu menuitem radio { color: @text@; background: none; border: none; box-shadow: none; }"
    "menu menuitem:hover check, menu menuitem:hover radio { color: @sel_fg@; }"

    // Buttons
    "button { background-color: @face@; background-image: none; color: @text@;"
    "  padding: 3px 10px; min-height: 18px; " RAISED " }"
    "button:hover { background-color: @face@; }"
    "button:active, button:checked { background-color: @face@; " PRESSED " }"
    "button:disabled { color: @disabled@; }"
    "button label { color: @text@; }"
    "button:focus { outline: 1px dotted @text@; outline-offset: -4px; }"

    // Text fields
    "entry { background-color: @well_bg@; color: @well_fg@; padding: 2px 4px; min-height: 18px;"
    "  border: 1px solid; border-color: @shadow@ @hilite@ @hilite@ @shadow@;"
    "  box-shadow: inset 1px 1px @dark@, inset -1px -1px @light@; background-image: none; caret-color: @well_fg@; }"
    "entry selection, label selection { background-color: @sel_bg@; color: @sel_fg@; }"
    "entry image { color: @well_fg@; }"

    // Tabs (editor + bottom panel)
    "notebook { background-color: @face@; }"
    "notebook > header { background-color: @face@; border: none; padding: 3px 3px 0 3px; box-shadow: none; }"
    "notebook > header tab { background-color: @face@; color: @text@; padding: 2px 10px; margin: 0 1px; min-height: 18px;"
    "  border: 1px solid; border-color: @hilite@ @dark@ @face@ @hilite@; box-shadow: inset -1px 0 @shadow@, inset 1px 1px @light@; }"
    "notebook > header tab label { color: @text@; }"
    "notebook > header tab:checked { background-color: @sel_bg@; }"
    "notebook > header tab:checked label { color: @sel_fg@; font-weight: bold; }"
    "notebook > header tab button.photon-tab-close { padding: 0 3px; min-height: 0; min-width: 0; margin: 0;"
    "  border: none; box-shadow: none; background: none; }"
    "notebook > header tab:checked button label { color: @sel_fg@; }"
    "notebook > header > tabs > arrow { color: @text@; }"
    "notebook > stack:not(:only-child) { background-color: @face@; }"

    // Win95 title-bar strip above the tabs
    ".photon-titlebar { background-image: linear-gradient(to right, @title_a@, @title_b@);"
    "  padding: 2px 6px; min-height: 18px; }"
    ".photon-titlebar label { color: @title_fg@; font-weight: bold; }"

    // Sunken wells: editor, sidebar, terminals
    "scrolledwindow.photon-sunken { background-color: @face@; " SUNKEN " }"

    // Sidebar
    "treeview.view { background-color: @well_bg@; color: @well_fg@; }"
    "treeview.view:hover { background-color: @well_bg@; }"
    "treeview.view:selected, treeview.view:selected:focus { background-color: @sel_bg@; color: @sel_fg@; }"
    "treeview.view.expander { color: @well_fg@; }"

    // Chunky Win95 scrollbars: track, bevelled thumb and arrow buttons
    "scrollbar { background-color: @track@; background-image: none; border: none; padding: 0;"
    "  -GtkScrollbar-has-backward-stepper: true; -GtkScrollbar-has-forward-stepper: true; }"
    "scrollbar slider { background-color: @face@; min-width: 14px; min-height: 14px; margin: 0; " RAISED " }"
    "scrollbar.vertical slider { min-height: 24px; }"
    "scrollbar.horizontal slider { min-width: 24px; }"
    "scrollbar button { background-color: @face@; color: @text@; min-width: 14px; min-height: 14px;"
    "  padding: 0; " RAISED " }"
    "scrollbar button:active { " PRESSED " }"
    "scrollbar button:disabled { color: @disabled@; }"
    "scrollbar contents trough { background-color: @track@; border: none; }"
    "scrolledwindow junction { background-color: @face@; }"

    // Splitters
    "paned > separator { background-color: @face@; background-image: none; min-width: 4px; min-height: 4px; }"

    // Status bar, search bar
    ".photon-statusbar { background-color: @face@; padding: 2px 6px; " RAISED " }"
    ".photon-statusbar > label { padding: 0 6px; border: 1px solid; border-color: @shadow@ @hilite@ @hilite@ @shadow@; }"
    ".photon-statusbar label.error { color: @error_fg@; font-weight: bold; }"
    ".photon-statusbar button { padding: 0 6px; min-height: 0; }"
    ".photon-search { background-color: @face@; border-bottom: 1px solid @shadow@; }"

    // Tooltips
    "tooltip, tooltip.background { background-color: @tip_bg@; border: 1px solid @dark@; box-shadow: none; }"
    "tooltip label { color: @tip_fg@; }"

    // Code: the theme's font, a wide see-through caret like a DOS cursor
    "textview { font-family: @code_font@; caret-color: @caret@; -GtkWidget-cursor-aspect-ratio: 0.55; }"

    // Completion popup
    ".gtksourcecompletion, .gtksourcecompletion treeview { background-color: @popup_bg@; color: @popup_fg@; }"
    ".gtksourcecompletion treeview:selected { background-color: @sel_bg@; color: @sel_fg@; }"

    // About dialog
    ".photon-about-box { background-color: @face@; padding: 12px; }"
    ".photon-about-well { background-color: @well_bg@; padding: 10px; " SUNKEN " }"
    ".photon-about-well label { color: @well_fg@; }";

// ---------------------------------------------------------------------
// Palettes for the template
// ---------------------------------------------------------------------
typedef struct {
    const gchar *name;
    const gchar *value;
} Var;

// Silver Windows 95 chrome, shared by Turbo 90s and Notepad 95
#define SILVER_CHROME \
    { "face", "#c0c0c0" }, { "light", "#dfdfdf" }, { "hilite", "#ffffff" }, { "shadow", "#808080" }, \
    { "dark", "#000000" }, { "text", "#000000" }, { "disabled", "#808080" },                         \
    { "sel_bg", "#000080" }, { "sel_fg", "#ffffff" },                                                 \
    { "title_a", "#000080" }, { "title_b", "#1084d0" }, { "title_fg", "#ffffff" },                    \
    { "well_bg", "#ffffff" }, { "well_fg", "#000000" }, { "track", "#dfdfdf" },                      \
    { "tip_bg", "#ffffe1" }, { "tip_fg", "#000000" }, { "error_fg", "#aa0000" },                     \
    { "ui_font", WIN95_FONTS }

// Turbo 90s: Win95 chrome around a Borland Turbo C++ blue screen
static const Var TURBO_VARS[] = {
    SILVER_CHROME,
    { "code_font", DOS_FONTS }, { "caret", "rgba(255, 255, 85, 0.65)" },
    { "popup_bg", "#00aaaa" }, { "popup_fg", "#000000" },
    { NULL, NULL }
};

// Notepad 95: the same chrome, black on white like the editors of the day
static const Var NOTEPAD_VARS[] = {
    SILVER_CHROME,
    { "code_font", FIXEDSYS_FONTS }, { "caret", "rgba(0, 0, 0, 0.75)" },
    { "popup_bg", "#ffffff" }, { "popup_fg", "#000000" },
    { NULL, NULL }
};

// A monochrome CRT: black glass, everything drawn in one phosphor color
#define CRT_CHROME(BRIGHT, MID, DIM)                                                                \
    { "face", "#000000" }, { "light", DIM }, { "hilite", MID }, { "shadow", DIM }, { "dark", DIM }, \
    { "text", MID }, { "disabled", DIM }, { "sel_bg", BRIGHT }, { "sel_fg", "#000000" },             \
    { "title_a", MID }, { "title_b", DIM }, { "title_fg", "#000000" },                               \
    { "well_bg", "#000000" }, { "well_fg", MID }, { "track", "#000000" },                            \
    { "tip_bg", "#000000" }, { "tip_fg", BRIGHT }, { "error_fg", BRIGHT },                           \
    { "ui_font", DOS_FONTS }, { "code_font", DOS_FONTS }, { "caret", BRIGHT },                       \
    { "popup_bg", "#000000" }, { "popup_fg", MID }

static const Var AMBER_VARS[] = { CRT_CHROME("#ffcc00", "#ffb000", "#7a5200"), { NULL, NULL } };
static const Var GREEN_VARS[] = { CRT_CHROME("#66ff66", "#33ff33", "#127a12"), { NULL, NULL } };

// Hot Dog Stand: the loudest color scheme Windows 3.1 ever shipped
static const Var HOTDOG_VARS[] = {
    { "face", "#ff0000" }, { "light", "#ff8080" }, { "hilite", "#ffff00" }, { "shadow", "#800000" },
    { "dark", "#000000" }, { "text", "#ffffff" }, { "disabled", "#ffb0b0" },
    { "sel_bg", "#000000" }, { "sel_fg", "#ffff00" },
    { "title_a", "#ffff00" }, { "title_b", "#ff0000" }, { "title_fg", "#000000" },
    { "well_bg", "#ffff00" }, { "well_fg", "#000000" }, { "track", "#ffff00" },
    { "tip_bg", "#ffff00" }, { "tip_fg", "#000000" }, { "error_fg", "#ffff00" },
    { "ui_font", WIN95_FONTS }, { "code_font", FIXEDSYS_FONTS }, { "caret", "rgba(0, 0, 0, 0.75)" },
    { "popup_bg", "#ffff00" }, { "popup_fg", "#000000" },
    { NULL, NULL }
};

// ---------------------------------------------------------------------
// VS Code Dark: the original look (plain CSS, no template)
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
    const gchar *const *css;      // CSS, or a template when `vars` is set
    const Var   *vars;            // palette for the template, or NULL
    const gchar *scheme;          // styles/<scheme>.xml id
    gboolean     dark;            // ask GTK for its dark variant underneath
    gboolean     title_strip;     // gradient strip above the tabs
    const gchar *term_fg;
    const gchar *term_bg;
    const gchar *term_font;       // Pango list, first installed family wins
    gboolean     term_mono;       // monochrome: every ANSI color is a shade of term_fg
    PhotonMarkerColors markers;   // error / warning lines in the editor
} ThemeInfo;

static const ThemeInfo THEMES[] = {
    {
        .id = "turbo90s", .name = "Turbo 90s",
        .css = &WIN95_CSS, .vars = TURBO_VARS, .scheme = "turbo90s",
        .title_strip = TRUE,
        .term_fg = "#aaaaaa", .term_bg = "#000000", .term_font = DOS_TERM_FONT,
        .markers = { "#aa0000", "#aa5500", "#ff5555", "#ffff55" },
    },
    {
        .id = "notepad95", .name = "Notepad 95",
        .css = &WIN95_CSS, .vars = NOTEPAD_VARS, .scheme = "notepad95",
        .title_strip = TRUE,
        .term_fg = "#c0c0c0", .term_bg = "#000000", .term_font = FIXEDSYS_TERM_FONT,
        .markers = { "#ffc0c0", "#ffffc0", "#ff0000", "#808000" },
    },
    {
        .id = "amber", .name = "Amber CRT",
        .css = &WIN95_CSS, .vars = AMBER_VARS, .scheme = "amber",
        .dark = TRUE, .title_strip = TRUE,
        .term_fg = "#ffb000", .term_bg = "#000000", .term_font = DOS_TERM_FONT, .term_mono = TRUE,
        .markers = { "#5a3a00", "#3a2600", "#ffcc00", "#b37a00" },
    },
    {
        .id = "green", .name = "Green Phosphor",
        .css = &WIN95_CSS, .vars = GREEN_VARS, .scheme = "green",
        .dark = TRUE, .title_strip = TRUE,
        .term_fg = "#33ff33", .term_bg = "#000000", .term_font = DOS_TERM_FONT, .term_mono = TRUE,
        .markers = { "#0d4d0d", "#0a330a", "#66ff66", "#1f9e1f" },
    },
    {
        .id = "hotdog", .name = "Hot Dog Stand",
        .css = &WIN95_CSS, .vars = HOTDOG_VARS, .scheme = "hotdog",
        .title_strip = TRUE,
        .term_fg = "#ffff00", .term_bg = "#800000", .term_font = FIXEDSYS_TERM_FONT,
        .markers = { "#ff8080", "#ffd080", "#000000", "#800000" },
    },
    {
        .id = "vscode-dark", .name = "VS Code Dark",
        .css = &VSCODE_CSS, .vars = NULL, .scheme = "vscode-dark",
        .dark = TRUE, .title_strip = FALSE,
        .term_fg = "#cccccc", .term_bg = "#1e1e1e",
        .term_font = "JetBrains Mono, Fira Code, Menlo, DejaVu Sans Mono, monospace 11",
        .markers = { "rgba(244,71,71,0.18)", "rgba(215,186,125,0.15)", "#f44747", "#d7ba7d" },
    },
};

static const gchar *const THEME_IDS[] = { "turbo90s", "notepad95", "amber", "green", "hotdog", "vscode-dark", NULL };

// Replace every @name@ in the template with the palette's value
static gchar *
fill_template(const gchar *template, const Var *vars) {
    GString *out = g_string_sized_new(strlen(template) + 2048);
    const gchar *p = template;

    while (*p != '\0') {
        const gchar *end = (*p == '@') ? strchr(p + 1, '@') : NULL;
        const Var *var = NULL;
        for (const Var *v = vars; end != NULL && v->name != NULL; v++) {
            if ((gsize)(end - p - 1) == strlen(v->name) && strncmp(p + 1, v->name, strlen(v->name)) == 0) var = v;
        }
        if (var != NULL) {
            g_string_append(out, var->value);
            p = end + 1;
        } else {
            g_string_append_c(out, *p++);
        }
    }
    return g_string_free(out, FALSE);
}

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

// 16 ANSI colors as shades of one phosphor: normal colors dimmed, bright
// ones at full strength, black stays black
static void
mono_palette(const GdkRGBA *fg, GdkRGBA palette[16]) {
    for (int i = 0; i < 16; i++) {
        gdouble level = (i == 0) ? 0.0 : (i == 8) ? 0.45 : (i < 8) ? 0.75 : 1.0;
        palette[i] = (GdkRGBA){ fg->red * level, fg->green * level, fg->blue * level, 1.0 };
    }
}

static void
style_terminal(PhotonTheme *theme, VteTerminal *terminal) {
    GdkRGBA fg, bg, palette[16];
    gdk_rgba_parse(&fg, theme->info->term_fg);
    gdk_rgba_parse(&bg, theme->info->term_bg);
    if (theme->info->term_mono) mono_palette(&fg, palette);
    vte_terminal_set_colors(terminal, &fg, &bg, theme->info->term_mono ? palette : NULL,
                            theme->info->term_mono ? 16 : 0);
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
    gchar *css = info->vars != NULL ? fill_template(*info->css, info->vars) : g_strdup(*info->css);
    GError *error = NULL;
    gboolean ok = gtk_css_provider_load_from_data(theme->chrome, css, -1, &error);
    if (!ok) {
        g_printerr("Theme '%s' CSS failed to load: %s\n", info->id, error->message);
        g_error_free(error);
    }
    g_free(css);

    for (GSList *l = theme->buffers; l != NULL; l = l->next) style_buffer(theme, l->data);
    for (GSList *l = theme->terminals; l != NULL; l = l->next) style_terminal(theme, l->data);
    return ok;
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

const PhotonMarkerColors *
photon_theme_marker_colors(PhotonTheme *theme) {
    return &theme->info->markers;
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
