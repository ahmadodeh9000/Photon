/**
 * @author: Ahmad Odeh
 * @date: 7th Nov, 2026
 *
**/
/*
 * fontdialog: the font picker. See fontdialog.h.
 */
#include "fontdialog.h"
#include "languages.h"

#define RESPONSE_RESET 1

// The characters people squint at in code fonts
#define CHOOSER_SAMPLE "0O 1lI| rn m {}()[] ;:., == != <= >= && || 'x' `y`"

static const gchar *const PREVIEW_LUNAR =
    "// Lunar\n"
    "struct Point {\n"
    "    init(x, y) { self.x = x; }\n"
    "}\n"
    "let m = { hp: 10, ok: true };\n"
    "print \"hp={m.hp} 0O 1lI\";\n";

static const gchar *const PREVIEW_C =
    "/* C */\n"
    "#include <stdio.h>\n"
    "int main(void) {\n"
    "    if (a != b && c <= 10) {}\n"
    "    printf(\"%d\\n\", x[0]);\n"
    "}\n";

static const gchar *const PREVIEW_PYTHON =
    "# Python\n"
    "def f(n: int) -> str:\n"
    "    return f\"{n ** 2}\"\n"
    "\n"
    "if x == 1 or y >= 2:\n"
    "    print([f(i) for i in range(3)])\n";

typedef struct {
    PhotonApp   *app;
    PhotonFonts  fonts;          // what's being picked (applied live)
    GtkWidget   *code_chooser;
    GtkWidget   *ui_chooser;
    GtkWidget   *show_all;       // "Show all fonts" for the code font
    gboolean     updating;       // setting the choosers ourselves: ignore their signals
} FontDialog;

// ---------------------------------------------------------------------
// Choosers
// ---------------------------------------------------------------------
static gboolean
only_monospace(const PangoFontFamily *family, const PangoFontFace *face G_GNUC_UNUSED, gpointer data G_GNUC_UNUSED) {
    return pango_font_family_is_monospace((PangoFontFamily *)family);
}

// Read a chooser into a family name + point size
static void
read_chooser(GtkWidget *chooser, gchar **family, gint *points) {
    PangoFontDescription *desc = gtk_font_chooser_get_font_desc(GTK_FONT_CHOOSER(chooser));
    if (desc == NULL) return;
    g_free(*family);
    *family = g_strdup(pango_font_description_get_family(desc));
    *points = pango_font_description_get_size(desc) / PANGO_SCALE;
    pango_font_description_free(desc);
}

// Point a chooser at a family + size; NULL family = leave the selection alone
static void
set_chooser(FontDialog *dialog, GtkWidget *chooser, const gchar *family, gint points) {
    dialog->updating = TRUE;
    if (family != NULL) {
        gchar *font = g_strdup_printf("%s %d", family, points);
        gtk_font_chooser_set_font(GTK_FONT_CHOOSER(chooser), font);
        g_free(font);
    } else {
        PangoFontDescription *desc = gtk_font_chooser_get_font_desc(GTK_FONT_CHOOSER(chooser));
        if (desc != NULL) {
            pango_font_description_set_size(desc, points * PANGO_SCALE);
            gtk_font_chooser_set_font_desc(GTK_FONT_CHOOSER(chooser), desc);
            pango_font_description_free(desc);
        }
    }
    dialog->updating = FALSE;
}

static void
on_code_font(GObject *chooser, GParamSpec *pspec G_GNUC_UNUSED, gpointer user_data) {
    FontDialog *dialog = user_data;
    if (dialog->updating) return;
    read_chooser(GTK_WIDGET(chooser), &dialog->fonts.code_family, &dialog->fonts.code_points);
    photon_app_set_fonts(dialog->app, &dialog->fonts, FALSE);   // live, not saved yet
}

static void
on_ui_font(GObject *chooser, GParamSpec *pspec G_GNUC_UNUSED, gpointer user_data) {
    FontDialog *dialog = user_data;
    if (dialog->updating) return;
    read_chooser(GTK_WIDGET(chooser), &dialog->fonts.ui_family, &dialog->fonts.ui_points);
    photon_app_set_fonts(dialog->app, &dialog->fonts, FALSE);
}

static void
on_show_all(GtkToggleButton *button, gpointer user_data) {
    FontDialog *dialog = user_data;
    gboolean all = gtk_toggle_button_get_active(button);
    gtk_font_chooser_set_filter_func(GTK_FONT_CHOOSER(dialog->code_chooser), all ? NULL : only_monospace, NULL, NULL);
}

static GtkWidget *
new_chooser(FontDialog *dialog, gboolean monospace_only, GCallback on_change) {
    GtkWidget *chooser = gtk_font_chooser_widget_new();
    gtk_font_chooser_set_level(GTK_FONT_CHOOSER(chooser),
        GTK_FONT_CHOOSER_LEVEL_FAMILY | GTK_FONT_CHOOSER_LEVEL_SIZE);
    gtk_font_chooser_set_preview_text(GTK_FONT_CHOOSER(chooser), CHOOSER_SAMPLE);
    if (monospace_only) gtk_font_chooser_set_filter_func(GTK_FONT_CHOOSER(chooser), only_monospace, NULL, NULL);
    gtk_widget_set_size_request(chooser, -1, 260);
    g_signal_connect(chooser, "notify::font", on_change, dialog);
    return chooser;
}

static GtkWidget *
code_page(FontDialog *dialog) {
    GtkWidget *page = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_container_set_border_width(GTK_CONTAINER(page), 6);
    dialog->code_chooser = new_chooser(dialog, TRUE, G_CALLBACK(on_code_font));
    dialog->show_all = gtk_check_button_new_with_label("Show all fonts (not only monospace)");
    g_signal_connect(dialog->show_all, "toggled", G_CALLBACK(on_show_all), dialog);
    gtk_box_pack_start(GTK_BOX(page), dialog->code_chooser, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(page), dialog->show_all, FALSE, FALSE, 0);
    return page;
}

static GtkWidget *
ui_page(FontDialog *dialog) {
    GtkWidget *page = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_container_set_border_width(GTK_CONTAINER(page), 6);
    dialog->ui_chooser = new_chooser(dialog, FALSE, G_CALLBACK(on_ui_font));
    gtk_box_pack_start(GTK_BOX(page), dialog->ui_chooser, TRUE, TRUE, 0);
    return page;
}

// The installed family `widget` really draws with: the first one in its
// CSS font list that exists (the theme's suggestion when nothing was picked)
static gchar *
family_in_use(GtkWidget *widget) {
    GtkStyleContext *style = gtk_widget_get_style_context(widget);
    PangoFontDescription *desc = NULL;
    gtk_style_context_get(style, gtk_style_context_get_state(style), "font", &desc, NULL);
    gchar **wanted = g_strsplit(desc != NULL ? pango_font_description_get_family(desc) : "", ",", -1);

    PangoFontFamily **families = NULL;
    int count = 0;
    pango_context_list_families(gtk_widget_get_pango_context(widget), &families, &count);
    gchar *found = NULL;
    for (gint i = 0; found == NULL && wanted[i] != NULL; i++) {
        gchar *name = g_strstrip(g_strdelimit(wanted[i], "'\"", ' '));
        for (int f = 0; found == NULL && f < count; f++) {
            if (g_ascii_strcasecmp(pango_font_family_get_name(families[f]), name) == 0) {
                found = g_strdup(pango_font_family_get_name(families[f]));
            }
        }
    }
    g_free(families);
    g_strfreev(wanted);
    if (desc != NULL) pango_font_description_free(desc);
    return found;
}

// ---------------------------------------------------------------------
// Preview: three small editors, styled like the real ones
// ---------------------------------------------------------------------
static GtkWidget *
preview_pane(PhotonApp *app, const gchar *gsv_lang, const gchar *code) {
    GtkSourceBuffer *buffer = gtk_source_buffer_new_with_language(photon_languages_get(gsv_lang));
    gtk_text_buffer_set_text(GTK_TEXT_BUFFER(buffer), code, -1);
    photon_theme_attach_buffer(app->theme, buffer);

    GtkWidget *view = gtk_source_view_new_with_buffer(buffer);
    gtk_text_view_set_editable(GTK_TEXT_VIEW(view), FALSE);
    gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(view), FALSE);
    gtk_source_view_set_show_line_numbers(GTK_SOURCE_VIEW(view), TRUE);
    g_object_unref(buffer);

    GtkWidget *scroll = gtk_scrolled_window_new(NULL, NULL);
    photon_theme_make_well(scroll);
    gtk_widget_set_size_request(scroll, 260, 150);
    gtk_container_add(GTK_CONTAINER(scroll), view);
    return scroll;
}

static GtkWidget *
preview(PhotonApp *app) {
    GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_box_pack_start(GTK_BOX(row), preview_pane(app, "lunar", PREVIEW_LUNAR), TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(row), preview_pane(app, "c", PREVIEW_C), TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(row), preview_pane(app, "python3", PREVIEW_PYTHON), TRUE, TRUE, 0);

    GtkWidget *frame = gtk_frame_new("Preview");
    gtk_container_set_border_width(GTK_CONTAINER(row), 6);
    gtk_container_add(GTK_CONTAINER(frame), row);
    return frame;
}

// ---------------------------------------------------------------------
// The dialog
// ---------------------------------------------------------------------
static void
reset(FontDialog *dialog) {
    photon_fonts_clear(&dialog->fonts);
    photon_app_set_fonts(dialog->app, &dialog->fonts, FALSE);
    set_chooser(dialog, dialog->code_chooser, NULL, PHOTON_FONT_SIZE_DEFAULT);
}

void
photon_font_dialog_run(PhotonApp *app) {
    FontDialog dialog = { .app = app };
    PhotonFonts original = { 0 };
    photon_fonts_copy(&original, photon_theme_get_fonts(app->theme));
    photon_fonts_copy(&dialog.fonts, &original);

    GtkWidget *window = gtk_dialog_new_with_buttons("Fonts", GTK_WINDOW(app->window),
        GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
        "_Reset to Default", RESPONSE_RESET, "_Cancel", GTK_RESPONSE_CANCEL, "_OK", GTK_RESPONSE_OK, NULL);
    gtk_dialog_set_default_response(GTK_DIALOG(window), GTK_RESPONSE_OK);

    GtkWidget *tabs = gtk_notebook_new();
    gtk_notebook_append_page(GTK_NOTEBOOK(tabs), code_page(&dialog), gtk_label_new("Code Font"));
    gtk_notebook_append_page(GTK_NOTEBOOK(tabs), ui_page(&dialog), gtk_label_new("Interface Font"));

    GtkWidget *content = gtk_dialog_get_content_area(GTK_DIALOG(window));
    gtk_container_set_border_width(GTK_CONTAINER(content), 8);
    gtk_box_set_spacing(GTK_BOX(content), 8);
    gtk_box_pack_start(GTK_BOX(content), tabs, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(content), preview(app), FALSE, FALSE, 0);

    // Show the current choice, or the fonts the theme uses if none was picked
    gchar *code_family = original.code_family != NULL ? g_strdup(original.code_family)
                       : family_in_use(GTK_WIDGET(photon_document_get_view(photon_app_current_doc(app))));
    gchar *ui_family = original.ui_family != NULL ? g_strdup(original.ui_family) : family_in_use(app->title_label);
    set_chooser(&dialog, dialog.code_chooser, code_family,
                original.code_points > 0 ? original.code_points : PHOTON_FONT_SIZE_DEFAULT);
    set_chooser(&dialog, dialog.ui_chooser, ui_family, original.ui_points > 0 ? original.ui_points : 10);
    g_free(code_family);
    g_free(ui_family);
    gtk_widget_show_all(window);

    gint response;
    while ((response = gtk_dialog_run(GTK_DIALOG(window))) == RESPONSE_RESET) reset(&dialog);

    if (response == GTK_RESPONSE_OK) photon_app_set_fonts(app, &dialog.fonts, TRUE);
    else photon_app_set_fonts(app, &original, FALSE);   // Cancel / closed: put it back

    gtk_widget_destroy(window);
    photon_fonts_clear(&dialog.fonts);
    photon_fonts_clear(&original);
}
