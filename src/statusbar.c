/**
 * @author: Ahmad Odeh
 * @date: 5th Nov, 2026
 *
**/
/*
 * statusbar: message + run status at the bottom. See statusbar.h.
 * Styled from theme.c through the "photon-statusbar" / "error" CSS classes.
 */
#include "statusbar.h"

struct PhotonStatusbar {
    GtkWidget *box;       // root widget
    GtkWidget *message;
    GtkWidget *run_status;
};

static GtkWidget *
new_label(void) {
    GtkWidget *label = gtk_label_new(NULL);
    gtk_label_set_xalign(GTK_LABEL(label), 0.0f);
    gtk_label_set_single_line_mode(GTK_LABEL(label), TRUE);
    return label;
}

PhotonStatusbar *
photon_statusbar_new(void) {
    PhotonStatusbar *bar = g_new0(PhotonStatusbar, 1);

    bar->box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_style_context_add_class(gtk_widget_get_style_context(bar->box), "photon-statusbar");
    g_object_ref_sink(bar->box);

    bar->message = new_label();
    gtk_label_set_ellipsize(GTK_LABEL(bar->message), PANGO_ELLIPSIZE_END);
    gtk_label_set_selectable(GTK_LABEL(bar->message), FALSE);
    bar->run_status = new_label();

    gtk_box_pack_start(GTK_BOX(bar->box), bar->message, TRUE, TRUE, 0);
    gtk_box_pack_end(GTK_BOX(bar->box), bar->run_status, FALSE, FALSE, 0);
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
photon_statusbar_set_run_status(PhotonStatusbar *bar, const gchar *text) {
    gtk_label_set_text(GTK_LABEL(bar->run_status), text != NULL ? text : "");
}
