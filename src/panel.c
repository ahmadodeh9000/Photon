/**
 * @author: Ahmad Odeh
 * @date: 5th Nov, 2026
 *
**/
/*
 * panel: bottom notebook with the shell terminal. See panel.h.
 */
#include "panel.h"
#include "platform.h"

#include <vte/vte.h>

struct PhotonPanel {
    GtkWidget *notebook;    // root widget
    GtkWidget *term_page;   // scrolled window holding the terminal
    GtkWidget *terminal;    // VteTerminal running the shell
    gchar     *cwd;         // where (re)spawned shells start
    gboolean   shell_dead;  // TRUE after the shell exits (e.g. Ctrl+D)
};

static void
spawn_shell(PhotonPanel *panel) {
    gchar *shell = vte_get_user_shell();   // zsh on macOS, usually bash on Linux
    if (shell == NULL) shell = g_strdup(photon_platform_fallback_shell());

    gchar *argv[] = { shell, NULL };
    vte_terminal_spawn_async(VTE_TERMINAL(panel->terminal),
        VTE_PTY_DEFAULT,
        panel->cwd,
        argv,
        NULL,               // envp: inherit
        G_SPAWN_DEFAULT,
        NULL, NULL, NULL,   // child setup + data + destroy
        -1,                 // default timeout
        NULL,               // cancellable
        NULL, NULL);        // callback + data

    g_free(shell);
    panel->shell_dead = FALSE;
}

static gboolean
terminal_tab_showing(PhotonPanel *panel) {
    gint current = gtk_notebook_get_current_page(GTK_NOTEBOOK(panel->notebook));
    return gtk_widget_get_visible(panel->notebook)
        && current == gtk_notebook_page_num(GTK_NOTEBOOK(panel->notebook), panel->term_page);
}

// Shell exited (Ctrl+D, `exit`, crash): remember it's dead and hide the panel
// if it was showing the terminal. The next toggle spawns a fresh shell;
// respawning right here would loop forever if the shell dies instantly.
static void
on_child_exited(VteTerminal *term G_GNUC_UNUSED, gint status G_GNUC_UNUSED, gpointer user_data) {
    PhotonPanel *panel = user_data;
    panel->shell_dead = TRUE;
    if (terminal_tab_showing(panel)) gtk_widget_hide(panel->notebook);
}

PhotonPanel *
photon_panel_new(const gchar *working_dir, PhotonTheme *theme) {
    PhotonPanel *panel = g_new0(PhotonPanel, 1);
    panel->cwd = g_strdup(working_dir);

    panel->notebook = gtk_notebook_new();
    gtk_notebook_set_show_border(GTK_NOTEBOOK(panel->notebook), FALSE);
    gtk_widget_set_size_request(panel->notebook, -1, 200);
    g_object_ref_sink(panel->notebook);

    panel->terminal = vte_terminal_new();
    photon_theme_attach_terminal(theme, VTE_TERMINAL(panel->terminal));
    g_signal_connect(panel->terminal, "child-exited", G_CALLBACK(on_child_exited), panel);

    panel->term_page = gtk_scrolled_window_new(NULL, NULL);
    gtk_container_add(GTK_CONTAINER(panel->term_page), panel->terminal);
    photon_theme_make_well(panel->term_page);
    photon_panel_add_tab(panel, panel->term_page, "Terminal");

    spawn_shell(panel);
    return panel;
}

void
photon_panel_free(PhotonPanel *panel) {
    if (panel == NULL) return;
    // The terminal may still emit child-exited while being destroyed
    g_signal_handlers_disconnect_by_data(panel->terminal, panel);
    g_object_unref(panel->notebook);
    g_free(panel->cwd);
    g_free(panel);
}

GtkWidget *
photon_panel_get_widget(PhotonPanel *panel) {
    return panel->notebook;
}

void
photon_panel_add_tab(PhotonPanel *panel, GtkWidget *content, const gchar *title) {
    gtk_notebook_append_page(GTK_NOTEBOOK(panel->notebook), content, gtk_label_new(title));
    gtk_widget_show_all(content);
}

void
photon_panel_show_tab(PhotonPanel *panel, GtkWidget *content) {
    gint page = gtk_notebook_page_num(GTK_NOTEBOOK(panel->notebook), content);
    if (page < 0) return;
    gtk_widget_show(panel->notebook);
    gtk_notebook_set_current_page(GTK_NOTEBOOK(panel->notebook), page);
}

void
photon_panel_toggle_terminal(PhotonPanel *panel) {
    if (terminal_tab_showing(panel)) {
        gtk_widget_hide(panel->notebook);
        return;
    }

    if (panel->shell_dead) {
        vte_terminal_reset(VTE_TERMINAL(panel->terminal), TRUE, TRUE);
        spawn_shell(panel);
    }
    photon_panel_show_tab(panel, panel->term_page);
    gtk_widget_grab_focus(panel->terminal);
}
