/**
 * @author: Ahmad Odeh
 * @date: 6th Nov, 2026
 *
**/
/*
 * remote: one Photon at a time. The running Photon listens on a Unix
 * socket in the user's runtime folder; the `photon` command (cli.c)
 * connects to it to open files there instead of starting a second copy.
 *
 * The protocol is one request per line, path = the rest of the line:
 *   open <path>      open (or switch to) a file
 *   run <path>       open it and run it
 *   wait <path>      open it; the reply "closed <path>" comes when its tab closes
 *   close <path>     close its tab (asks first if it has unsaved changes)
 *   list             reply: one "file <path>" per open tab, then "end"
 *   present          bring the window to the front
 * Only GLib / GIO: the client side never touches GTK.
 */
#ifndef PHOTON_REMOTE_H
#define PHOTON_REMOTE_H

#include <gio/gio.h>

typedef struct PhotonRemote PhotonRemote;

// What the running Photon does for each request (app.c fills these in)
typedef struct {
    void    (*open)(const gchar *path, gboolean run, gpointer user_data);
    void    (*close)(const gchar *path, gpointer user_data);
    gchar **(*list)(gpointer user_data);   // NULL-terminated, caller frees
    void    (*present)(gpointer user_data);
    gpointer user_data;
} PhotonRemoteHandlers;

// Where the socket lives (newly allocated).
gchar *photon_remote_socket_path(void);

// ---- Server (the running Photon) -------------------------------------

// Start listening. Returns NULL if another Photon already owns the socket.
PhotonRemote *photon_remote_serve(const PhotonRemoteHandlers *handlers);

// Stops listening, tells every waiting client, removes the socket.
void photon_remote_free(PhotonRemote *remote);

// A tab was closed: answers the clients waiting on `path`.
void photon_remote_document_closed(PhotonRemote *remote, const gchar *path);

// ---- Client (the photon command) --------------------------------------

// Connect to the running Photon; NULL if there is none.
GSocketConnection *photon_remote_connect(void);

// Keep trying for up to `seconds` (while a new Photon starts up).
GSocketConnection *photon_remote_connect_wait(gdouble seconds);

// Send one request line ("open", "/abs/path").
gboolean photon_remote_send(GSocketConnection *connection, const gchar *verb, const gchar *path);

// Read one reply line (without the newline); NULL at end of stream.
gchar *photon_remote_read_line(GSocketConnection *connection);

#endif
