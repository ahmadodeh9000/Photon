/**
 * @author: Ahmad Odeh
 * @date: 5th Nov, 2026
 *
**/
/*
 * runner: run the current file. Saves it, builds the command from the
 * language's run_cmd, runs it in the panel's "Run" tab (a second
 * VteTerminal, so input()/scanf/os.input() and SDL windows work), tees
 * stderr to a temp file, then parses it into error markers. Shows the exit
 * code and elapsed time. Also finds the tools (lunar, python3, cc).
 */
#ifndef PHOTON_RUNNER_H
#define PHOTON_RUNNER_H

#include <glib.h>

typedef struct PhotonApp PhotonApp;
typedef struct PhotonRunner PhotonRunner;

// Adds the "Run" tab to app->panel. The app must outlive the runner.
PhotonRunner *photon_runner_new(PhotonApp *app);
void          photon_runner_free(PhotonRunner *runner);

void     photon_runner_run(PhotonRunner *runner);    // run the current document
void     photon_runner_stop(PhotonRunner *runner);   // SIGTERM, then SIGKILL
gboolean photon_runner_is_running(PhotonRunner *runner);

// Ask for the lunar binary with a file chooser and remember it in settings.
// Returns the chosen path (newly allocated) or NULL if cancelled.
gchar *photon_runner_locate_lunar(PhotonRunner *runner);

#endif
