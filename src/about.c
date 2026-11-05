/**
 * @author: Ahmad Odeh
 * @date: 5th Nov, 2026
 *
**/
/*
 * about: the About Photon dialog. See about.h.
 */
#include "about.h"

#ifndef PHOTON_VERSION
#define PHOTON_VERSION "dev"
#endif

static GtkWidget *
title_strip(const gchar *text) {
    GtkWidget *box = gtk_event_box_new();
    GtkWidget *label = gtk_label_new(text);
    gtk_label_set_xalign(GTK_LABEL(label), 0.0f);
    gtk_container_add(GTK_CONTAINER(box), label);
    gtk_style_context_add_class(gtk_widget_get_style_context(box), "photon-titlebar");
    return box;
}

static GtkWidget *
line(const gchar *markup) {
    GtkWidget *label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), markup);
    gtk_label_set_xalign(GTK_LABEL(label), 0.0f);
    return label;
}

// The program icon, 48 px, if the icon theme has it
static GtkWidget *
icon(void) {
    GdkPixbuf *pixbuf = gtk_icon_theme_load_icon(gtk_icon_theme_get_default(), "photon", 48,
                                                 GTK_ICON_LOOKUP_FORCE_SIZE, NULL);
    GtkWidget *image = pixbuf != NULL ? gtk_image_new_from_pixbuf(pixbuf) : gtk_image_new();
    if (pixbuf != NULL) g_object_unref(pixbuf);
    gtk_widget_set_valign(image, GTK_ALIGN_START);
    return image;
}

static GtkWidget *
info_well(void) {
    GtkWidget *well = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_style_context_add_class(gtk_widget_get_style_context(well), "photon-about-well");
    gtk_box_pack_start(GTK_BOX(well), line("<b>Photon</b>"), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(well), line("Version " PHOTON_VERSION), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(well), line("A small editor for Lunar, C and Python."), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(well), line("Copyright \xc2\xa9 2026 Ahmad Odeh"), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(well), line("Built with GTK, GtkSourceView and VTE."), FALSE, FALSE, 0);
    return well;
}

void
photon_about_show(GtkWindow *parent) {
    GtkWidget *dialog = gtk_dialog_new_with_buttons("About Photon", parent,
        GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT, "OK", GTK_RESPONSE_OK, NULL);
    gtk_window_set_resizable(GTK_WINDOW(dialog), FALSE);

    GtkWidget *body = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 14);
    gtk_style_context_add_class(gtk_widget_get_style_context(body), "photon-about-box");
    gtk_box_pack_start(GTK_BOX(body), icon(), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(body), info_well(), TRUE, TRUE, 0);

    GtkWidget *content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    gtk_box_pack_start(GTK_BOX(content), title_strip("About Photon"), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(content), body, TRUE, TRUE, 0);
    gtk_widget_show_all(dialog);

    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
}
