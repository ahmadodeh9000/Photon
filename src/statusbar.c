/**
 * @author: Ahmad Odeh
 * @date: 5th Nov, 2026
 *
**/
/*
 * statusbar: message, cursor position, language picker, run status.
 * See statusbar.h. Styled from theme.c through the "photon-statusbar" and
 * "error" CSS classes.
 */
#include "statusbar.h"

#define DATA_LANGUAGE "photon-language"

struct PhotonStatusbar {
    GtkWidget *box;          // root widget
    GtkWidget *message;
    GtkWidget *position;
    GtkWidget *language;     // GtkMenuButton
    GtkWidget *run_status;

    PhotonStatusbarLanguageFunc on_language;
    gpointer                    user_data;
};

static GtkWidget *
new_label(void) {
    GtkWidget *label = gtk_label_new(NULL);
    gtk_label_set_xalign(GTK_LABEL(label), 0.0f);
    gtk_label_set_single_line_mode(GTK_LABEL(label), TRUE);
    return label;
}

static void
on_language_item(GtkMenuItem *item, gpointer user_data) {
    PhotonStatusbar *bar = user_data;
    if (bar->on_language != NULL) {
        bar->on_language(g_object_get_data(G_OBJECT(item), DATA_LANGUAGE), bar->user_data);
    }
}

static void
add_language_item(PhotonStatusbar *bar, GtkWidget *menu, const gchar *name, const PhotonLanguage *language) {
    GtkWidget *item = gtk_menu_item_new_with_label(name);
    g_object_set_data(G_OBJECT(item), DATA_LANGUAGE, (gpointer)language);
    g_signal_connect(item, "activate", G_CALLBACK(on_language_item), bar);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
}

// Every registry language, then Plain Text
static GtkWidget *
build_language_button(PhotonStatusbar *bar) {
    GtkWidget *menu = gtk_menu_new();
    const PhotonLanguage *const *all = photon_languages_all();
    for (int i = 0; all[i] != NULL; i++) add_language_item(bar, menu, all[i]->name, all[i]);
    add_language_item(bar, menu, "Plain Text", NULL);
    gtk_widget_show_all(menu);

    GtkWidget *button = gtk_menu_button_new();
    gtk_menu_button_set_popup(GTK_MENU_BUTTON(button), menu);
    gtk_menu_button_set_direction(GTK_MENU_BUTTON(button), GTK_ARROW_UP);
    gtk_button_set_relief(GTK_BUTTON(button), GTK_RELIEF_NONE);
    gtk_widget_set_focus_on_click(button, FALSE);
    gtk_widget_set_tooltip_text(button, "Change the language");
    gtk_button_set_label(GTK_BUTTON(button), "Plain Text");
    return button;
}

PhotonStatusbar *
photon_statusbar_new(PhotonStatusbarLanguageFunc on_language, gpointer user_data) {
    PhotonStatusbar *bar = g_new0(PhotonStatusbar, 1);
    bar->on_language = on_language;
    bar->user_data = user_data;

    bar->box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 14);
    gtk_style_context_add_class(gtk_widget_get_style_context(bar->box), "photon-statusbar");
    g_object_ref_sink(bar->box);

    bar->message = new_label();
    gtk_label_set_ellipsize(GTK_LABEL(bar->message), PANGO_ELLIPSIZE_END);
    bar->position = new_label();
    bar->language = build_language_button(bar);
    bar->run_status = new_label();

    gtk_box_pack_start(GTK_BOX(bar->box), bar->message, TRUE, TRUE, 0);
    gtk_box_pack_end(GTK_BOX(bar->box), bar->run_status, FALSE, FALSE, 0);
    gtk_box_pack_end(GTK_BOX(bar->box), bar->language, FALSE, FALSE, 0);
    gtk_box_pack_end(GTK_BOX(bar->box), bar->position, FALSE, FALSE, 0);
    return bar;
}

void
photon_statusbar_free(PhotonStatusbar *bar) {
    if (bar == NULL) return;
    g_object_unref(bar->box);
    g_free(bar);
}

GtkWidget *
photon_statusbar_get_widget(PhotonStatusbar *bar) {
    return bar->box;
}

void
photon_statusbar_set_message(PhotonStatusbar *bar, const gchar *text, gboolean is_error) {
    GtkStyleContext *style = gtk_widget_get_style_context(bar->message);
    if (is_error) gtk_style_context_add_class(style, "error");
    else gtk_style_context_remove_class(style, "error");

    gtk_label_set_text(GTK_LABEL(bar->message), text != NULL ? text : "");
    gtk_widget_set_tooltip_text(bar->message, text);
}

void
photon_statusbar_set_position(PhotonStatusbar *bar, gint line, gint column) {
    gchar *text = g_strdup_printf("Ln %d, Col %d", line, column);
    gtk_label_set_text(GTK_LABEL(bar->position), text);
    g_free(text);
}

void
photon_statusbar_set_language(PhotonStatusbar *bar, const gchar *name) {
    gtk_button_set_label(GTK_BUTTON(bar->language), name);
}

void
photon_statusbar_set_run_status(PhotonStatusbar *bar, const gchar *text) {
    gtk_label_set_text(GTK_LABEL(bar->run_status), text != NULL ? text : "");
}
