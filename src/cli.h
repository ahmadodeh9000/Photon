/**
 * @author: Ahmad Odeh
 * @date: 6th Nov, 2026
 *
**/
/*
 * cli: the command line. Parses it (for both modes), and is the launcher
 * front-end of the `photon` command, which behaves like VS Code's `code`:
 *
 *   photon [FILE...]   returns at once, prints nothing; the files open in
 *                      the running Photon, or in a new one started detached
 *                      from the terminal (its output goes to a log file)
 *   photon -w FILE     waits until FILE's tab is closed (git core.editor)
 *   photon FOLDER      opens a new window rooted at FOLDER
 *   photon --version / --help / --status
 *
 * The window itself runs in "GUI mode" (--detached, --foreground, or the
 * test flags); main.c decides which. The launcher never initializes GTK.
 */
#ifndef PHOTON_CLI_H
#define PHOTON_CLI_H

#include <glib.h>

typedef struct {
    gboolean   help;
    gboolean   version;
    gboolean   status;       // --status: list the running Photon's open files
    gboolean   wait;         // -w / --wait
    gboolean   run;          // --run: run the (first) file once it's open
    gboolean   foreground;   // --foreground: GUI in this process, output here
    gboolean   detached;     // --detached: GUI in this process, output to the log
    gboolean   smoke_test;   // --smoke-test / --screenshot (GUI, foreground)
    gchar     *screenshot;
    gchar     *theme;
    GPtrArray *paths;        // absolute paths, in order
} PhotonArgs;

// Fills `args`; FALSE (with a message in *error) on a bad command line.
gboolean photon_args_parse(int argc, char **argv, PhotonArgs *args, GError **error);
void     photon_args_clear(PhotonArgs *args);

// Should this process open the window itself?
gboolean photon_args_wants_gui(const PhotonArgs *args);

void photon_cli_print_usage(void);

// Where a detached Photon writes its output.
gchar *photon_cli_log_path(void);

// The launcher: hand the request to a running Photon or start one. Returns
// the process exit code.
int photon_cli_run(const PhotonArgs *args);

#endif
