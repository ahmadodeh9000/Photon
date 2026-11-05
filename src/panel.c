/**
 * @author: Ahmad Odeh
 * @date: 5th Nov, 2026
 *
**/
/*
 * panel: bottom terminal panel. See panel.h.
 */
#include "panel.h"
#include "platform.h"

#include <vte/vte.h>

struct PhotonPanel {
    GtkWidget *root;        // scrolled window
    GtkWidget *terminal;    // VteTerminal
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

// Shell exited (Ctrl+D, `exit`, crash): hide the pane and remember it's dead.
// The next toggle spawns a fresh shell; respawning right here would loop
// forever if the shell dies instantly.
static void
on_child_exited(VteTerminal *term G_GNUC_UNUSED, gint status G_GNUC_UNUSED, gpointer user_data) {
    PhotonPanel *panel = user_data;
    panel->shell_dead = TRUE;
    gtk_widget_hide(panel->root);
}

PhotonPanel *
photon_panel_new(const gchar *working_dir) {
    PhotonPanel *panel = g_new0(PhotonPanel, 1);
    panel->cwd = g_strdup(working_dir);

    panel->terminal = vte_terminal_new();
    GdkRGBA fg, bg;
    gdk_rgba_parse(&fg, "#cccccc");
    gdk_rgba_parse(&bg, "#1e1e1e");
    vte_terminal_set_colors(VTE_TERMINAL(panel->terminal), &fg, &bg, NULL, 0);
    g_signal_connect(panel->terminal, "child-exited", G_CALLBACK(on_child_exited), panel);

    panel->root = gtk_scrolled_window_new(NULL, NULL);
    gtk_container_add(GTK_CONTAINER(panel->root), panel->terminal);
    gtk_widget_set_size_request(panel->root, -1, 200);
    g_object_ref_sink(panel->root);

    spawn_shell(panel);
    return panel;
}

void
photon_panel_free(PhotonPanel *panel) {
    if (panel == NULL) return;
    // The terminal may still emit child-exited while being destroyed
    g_signal_handlers_disconnect_by_data(panel->terminal, panel);
    g_object_unref(panel->root);
    g_free(panel->cwd);
    g_free(panel);
}

GtkWidget *
photon_panel_get_widget(PhotonPanel *panel) {
    return panel->root;
}

void
photon_panel_toggle(PhotonPanel *panel) {
    if (gtk_widget_get_visible(panel->root)) {
        gtk_widget_hide(panel->root);
        return;
    }

    if (panel->shell_dead) {
        vte_terminal_reset(VTE_TERMINAL(panel->terminal), TRUE, TRUE);
        spawn_shell(panel);
    }
    gtk_widget_show(panel->root);
    gtk_widget_grab_focus(panel->terminal);
}
