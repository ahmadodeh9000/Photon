/**
 * @author: Ahmad Odeh
 * @date: 6th Nov, 2026
 *
**/
/*
 * remote: single-instance socket, server and client. See remote.h.
 */
#include "remote.h"
#include "platform.h"

#include <gio/gunixsocketaddress.h>
#include <glib/gstdio.h>
#include <string.h>

// sun_path is 104 bytes on macOS, 108 on Linux
#define SOCKET_PATH_MAX 100

#define DATA_READER  "photon-reader"
#define DATA_CANCEL  "photon-cancel"    // cancels the connection's pending read
#define DATA_READING "photon-reading"   // a read is pending
#define DATA_CLOSING "photon-closing"   // close once the pending read returns

struct PhotonRemote {
    gchar               *path;
    PhotonRemoteHandlers handlers;

    // The server thread and everything only it touches
    GThread             *thread;
    GMainContext        *context;
    GMainLoop           *loop;
    GSocketService      *service;
    GList               *waiters;   // Waiter*: clients waiting for a tab to close
    GList               *clients;   // GSocketConnection*: every open connection

    // serve() waits for the thread to report: 1 listening, -1 failed
    GMutex               lock;
    GCond                started;
    gint                 state;
};

typedef struct {
    GSocketConnection *connection;
    gchar             *path;
} Waiter;

gchar *
photon_remote_socket_path(void) {
    gchar *dir = photon_platform_runtime_dir();
    gchar *path = g_build_filename(dir, "photon.sock", NULL);

    if (strlen(path) > SOCKET_PATH_MAX) {   // runtime dir too deep: fall back to tmp
        g_free(path);
        g_free(dir);
        gchar *name = g_strdup_printf("photon-%s", g_get_user_name());
        dir = g_build_filename(g_get_tmp_dir(), name, NULL);
        path = g_build_filename(dir, "photon.sock", NULL);
        g_free(name);
    }
    g_mkdir_with_parents(dir, 0700);
    g_free(dir);
    return path;
}

// ---------------------------------------------------------------------
// Client
// ---------------------------------------------------------------------
static GSocketConnection *
connect_to(const gchar *path) {
    if (!g_file_test(path, G_FILE_TEST_EXISTS)) return NULL;

    GSocketAddress *address = g_unix_socket_address_new(path);
    GSocketClient *client = g_socket_client_new();
    GSocketConnection *connection = g_socket_client_connect(client, G_SOCKET_CONNECTABLE(address), NULL, NULL);
    g_object_unref(client);
    g_object_unref(address);
    return connection;
}

GSocketConnection *
photon_remote_connect(void) {
    gchar *path = photon_remote_socket_path();
    GSocketConnection *connection = connect_to(path);
    g_free(path);
    return connection;
}

GSocketConnection *
photon_remote_connect_wait(gdouble seconds) {
    gint64 deadline = g_get_monotonic_time() + (gint64)(seconds * G_USEC_PER_SEC);
    GSocketConnection *connection;
    while ((connection = photon_remote_connect()) == NULL && g_get_monotonic_time() < deadline) {
        g_usleep(100 * 1000);
    }
    return connection;
}

gboolean
photon_remote_send(GSocketConnection *connection, const gchar *verb, const gchar *path) {
    if (path != NULL && strchr(path, '\n') != NULL) return FALSE;   // can't be sent on one line
    gchar *line = path != NULL ? g_strdup_printf("%s %s\n", verb, path) : g_strdup_printf("%s\n", verb);
    GOutputStream *out = g_io_stream_get_output_stream(G_IO_STREAM(connection));
    gboolean ok = g_output_stream_write_all(out, line, strlen(line), NULL, NULL, NULL);
    g_free(line);
    return ok;
}

gchar *
photon_remote_read_line(GSocketConnection *connection) {
    GDataInputStream *reader = g_object_get_data(G_OBJECT(connection), DATA_READER);
    if (reader == NULL) {
        reader = g_data_input_stream_new(g_io_stream_get_input_stream(G_IO_STREAM(connection)));
        g_object_set_data_full(G_OBJECT(connection), DATA_READER, reader, g_object_unref);
    }
    return g_data_input_stream_read_line_utf8(reader, NULL, NULL, NULL);
}

// ---------------------------------------------------------------------
// Server
//
// The sockets live in a thread of their own with its own GMainContext:
// GTK's macOS event loop polls file descriptors from a helper thread and
// trips over sockets opening and closing on the main loop. Requests reach
// the app as one-way calls on the main thread (MainCall); "a tab closed"
// comes back the same way the other direction.
// ---------------------------------------------------------------------
typedef struct {
    PhotonRemote      *remote;
    gchar             *verb;
    gchar             *path;         // NULL for list / present
    GSocketConnection *connection;   // for replies (list)
} MainCall;

static void
reply(GSocketConnection *connection, const gchar *line) {
    GOutputStream *out = g_io_stream_get_output_stream(G_IO_STREAM(connection));
    gchar *text = g_strconcat(line, "\n", NULL);
    g_output_stream_write_all(out, text, strlen(text), NULL, NULL, NULL);   // a gone client is fine
    g_free(text);
}

// ---- main thread: run the request in the app --------------------------
static gboolean
main_call(gpointer user_data) {
    MainCall *call = user_data;
    const PhotonRemoteHandlers *h = &call->remote->handlers;

    if (call->path != NULL && (g_str_equal(call->verb, "open") || g_str_equal(call->verb, "run")
                               || g_str_equal(call->verb, "wait"))) {
        h->open(call->path, g_str_equal(call->verb, "run"), h->user_data);
        h->present(h->user_data);
    } else if (call->path != NULL && g_str_equal(call->verb, "close")) {
        h->close(call->path, h->user_data);
    } else if (g_str_equal(call->verb, "list")) {
        gchar **files = h->list(h->user_data);
        for (gint i = 0; files != NULL && files[i] != NULL; i++) {
            gchar *entry = g_strconcat("file ", files[i], NULL);
            reply(call->connection, entry);
            g_free(entry);
        }
        reply(call->connection, "end");
        g_strfreev(files);
    } else if (g_str_equal(call->verb, "present")) {
        h->present(h->user_data);
    } else {
        g_printerr("remote: unknown request: %s\n", call->verb);
    }
    return G_SOURCE_REMOVE;
}

static void
main_call_free(gpointer user_data) {
    MainCall *call = user_data;
    g_object_unref(call->connection);
    g_free(call->verb);
    g_free(call->path);
    g_free(call);
}

// ---- server thread ----------------------------------------------------
static gboolean
has_waiters(PhotonRemote *remote, GSocketConnection *connection) {
    for (GList *l = remote->waiters; l != NULL; l = l->next) {
        if (((Waiter *)l->data)->connection == connection) return TRUE;
    }
    return FALSE;
}

static void
close_connection(GSocketConnection *connection) {
    g_io_stream_close(G_IO_STREAM(connection), NULL, NULL);
    g_object_unref(connection);   // the clients list's reference
}

// Never close a socket under a pending read (its fd is still being
// polled): cancel the read, and on_line() closes it when it returns
static void
drop_client(PhotonRemote *remote, GSocketConnection *connection) {
    if (g_list_find(remote->clients, connection) == NULL) return;
    remote->clients = g_list_remove(remote->clients, connection);
    if (g_object_get_data(G_OBJECT(connection), DATA_READING) != NULL) {
        g_object_set_data(G_OBJECT(connection), DATA_CLOSING, GINT_TO_POINTER(1));
        g_cancellable_cancel(g_object_get_data(G_OBJECT(connection), DATA_CANCEL));
    } else {
        close_connection(connection);
    }
}

static void
handle_request(PhotonRemote *remote, GSocketConnection *connection, const gchar *line) {
    const gchar *space = strchr(line, ' ');
    MainCall *call = g_new0(MainCall, 1);
    call->remote = remote;
    call->verb = space != NULL ? g_strndup(line, (gsize)(space - line)) : g_strdup(line);
    call->path = space != NULL ? g_strdup(space + 1) : NULL;
    call->connection = g_object_ref(connection);

    if (call->path != NULL && g_str_equal(call->verb, "wait")) {
        Waiter *waiter = g_new0(Waiter, 1);
        waiter->connection = connection;
        waiter->path = g_canonicalize_filename(call->path, NULL);
        remote->waiters = g_list_append(remote->waiters, waiter);
    }
    g_main_context_invoke_full(NULL, G_PRIORITY_DEFAULT, main_call, call, main_call_free);
}

static void read_next(GSocketConnection *connection);

static void
on_line(GObject *source, GAsyncResult *result, gpointer user_data) {
    GSocketConnection *connection = user_data;
    PhotonRemote *remote = g_object_get_data(G_OBJECT(connection), "photon-remote");
    gchar *line = g_data_input_stream_read_line_finish_utf8(G_DATA_INPUT_STREAM(source), result, NULL, NULL);
    g_object_set_data(G_OBJECT(connection), DATA_READING, NULL);

    if (g_object_get_data(G_OBJECT(connection), DATA_CLOSING) != NULL) {   // dropped meanwhile
        g_free(line);
        close_connection(connection);
        g_object_unref(connection);
        return;
    }
    if (line == NULL) {                       // client finished sending (or the server stopped)
        if (remote != NULL && !has_waiters(remote, connection)) drop_client(remote, connection);
    } else {
        if (remote != NULL && *line != '\0') handle_request(remote, connection, line);
        if (remote != NULL) read_next(connection);
        g_free(line);
    }
    g_object_unref(connection);
}

static void
read_next(GSocketConnection *connection) {
    GDataInputStream *reader = g_object_get_data(G_OBJECT(connection), DATA_READER);
    GCancellable *cancel = g_object_get_data(G_OBJECT(connection), DATA_CANCEL);
    g_object_set_data(G_OBJECT(connection), DATA_READING, GINT_TO_POINTER(1));
    g_data_input_stream_read_line_async(reader, G_PRIORITY_DEFAULT, cancel, on_line, g_object_ref(connection));
}

static gboolean
on_incoming(GSocketService *service G_GNUC_UNUSED, GSocketConnection *connection,
            GObject *source G_GNUC_UNUSED, gpointer user_data) {
    PhotonRemote *remote = user_data;
    GDataInputStream *reader = g_data_input_stream_new(g_io_stream_get_input_stream(G_IO_STREAM(connection)));
    g_object_set_data_full(G_OBJECT(connection), DATA_READER, reader, g_object_unref);
    g_object_set_data_full(G_OBJECT(connection), DATA_CANCEL, g_cancellable_new(), g_object_unref);
    g_object_set_data(G_OBJECT(connection), "photon-remote", remote);

    remote->clients = g_list_prepend(remote->clients, g_object_ref(connection));
    read_next(connection);
    return TRUE;
}

// Listen on `path`; a leftover socket from a crashed Photon is replaced
static gboolean
listen_on(GSocketService *service, const gchar *path) {
    GSocketConnection *alive = connect_to(path);
    if (alive != NULL) {             // another Photon answers: it owns the socket
        g_object_unref(alive);
        return FALSE;
    }
    g_unlink(path);

    GSocketAddress *address = g_unix_socket_address_new(path);
    GError *error = NULL;
    gboolean ok = g_socket_listener_add_address(G_SOCKET_LISTENER(service), address,
        G_SOCKET_TYPE_STREAM, G_SOCKET_PROTOCOL_DEFAULT, NULL, NULL, &error);
    if (!ok) {
        g_printerr("remote: can't listen on %s: %s\n", path, error->message);
        g_error_free(error);
    }
    g_object_unref(address);
    return ok;
}

// Tell serve() whether listening worked
static void
report_started(PhotonRemote *remote, gint state) {
    g_mutex_lock(&remote->lock);
    remote->state = state;
    g_cond_signal(&remote->started);
    g_mutex_unlock(&remote->lock);
}

static gpointer
server_thread(gpointer user_data) {
    PhotonRemote *remote = user_data;
    g_main_context_push_thread_default(remote->context);

    remote->service = g_socket_service_new();
    if (!listen_on(remote->service, remote->path)) {
        g_clear_object(&remote->service);
        g_main_context_pop_thread_default(remote->context);
        report_started(remote, -1);
        return NULL;
    }
    g_signal_connect(remote->service, "incoming", G_CALLBACK(on_incoming), remote);
    g_socket_service_start(remote->service);
    report_started(remote, 1);

    g_main_loop_run(remote->loop);

    g_clear_object(&remote->service);
    g_main_context_pop_thread_default(remote->context);
    return NULL;
}

PhotonRemote *
photon_remote_serve(const PhotonRemoteHandlers *handlers) {
    PhotonRemote *remote = g_new0(PhotonRemote, 1);
    remote->path = photon_remote_socket_path();
    remote->handlers = *handlers;
    remote->context = g_main_context_new();
    remote->loop = g_main_loop_new(remote->context, FALSE);
    g_mutex_init(&remote->lock);
    g_cond_init(&remote->started);

    remote->thread = g_thread_new("photon-remote", server_thread, remote);
    g_mutex_lock(&remote->lock);
    while (remote->state == 0) g_cond_wait(&remote->started, &remote->lock);
    gboolean listening = remote->state > 0;
    g_mutex_unlock(&remote->lock);

    if (!listening) {   // another Photon owns the socket
        g_thread_join(remote->thread);
        g_main_loop_unref(remote->loop);
        g_main_context_unref(remote->context);
        g_mutex_clear(&remote->lock);
        g_cond_clear(&remote->started);
        g_free(remote->path);
        g_free(remote);
        return NULL;
    }
    return remote;
}

// ---- "a tab closed": main thread -> server thread ---------------------
typedef struct {
    PhotonRemote *remote;
    gchar        *path;
} Closed;

static gboolean
server_closed(gpointer user_data) {
    Closed *closed = user_data;
    PhotonRemote *remote = closed->remote;

    GList *l = remote->waiters;
    while (l != NULL) {
        GList *next = l->next;
        Waiter *waiter = l->data;
        if (g_str_equal(waiter->path, closed->path)) {
            gchar *line = g_strconcat("closed ", waiter->path, NULL);
            reply(waiter->connection, line);
            g_free(line);
            remote->waiters = g_list_delete_link(remote->waiters, l);
            GSocketConnection *connection = waiter->connection;
            g_free(waiter->path);
            g_free(waiter);
            if (!has_waiters(remote, connection)) drop_client(remote, connection);
        }
        l = next;
    }
    return G_SOURCE_REMOVE;
}

static void
closed_free(gpointer user_data) {
    Closed *closed = user_data;
    g_free(closed->path);
    g_free(closed);
}

void
photon_remote_document_closed(PhotonRemote *remote, const gchar *path) {
    if (remote == NULL || path == NULL) return;
    Closed *closed = g_new0(Closed, 1);
    closed->remote = remote;
    closed->path = g_canonicalize_filename(path, NULL);
    g_main_context_invoke_full(remote->context, G_PRIORITY_DEFAULT, server_closed, closed, closed_free);
}

// ---- shutdown: runs in the server thread ------------------------------
static gboolean
quit_loop(gpointer loop) {
    g_main_loop_quit(loop);
    return G_SOURCE_REMOVE;
}

static gboolean
server_stop(gpointer user_data) {
    PhotonRemote *remote = user_data;
    g_socket_service_stop(remote->service);
    g_socket_listener_close(G_SOCKET_LISTENER(remote->service));

    // Waiting clients see the end of the stream and exit
    for (GList *l = remote->waiters; l != NULL; l = l->next) {
        g_free(((Waiter *)l->data)->path);
        g_free(l->data);
    }
    g_clear_pointer(&remote->waiters, g_list_free);
    while (remote->clients != NULL) {
        GSocketConnection *connection = remote->clients->data;
        g_object_set_data(G_OBJECT(connection), "photon-remote", NULL);
        drop_client(remote, connection);
    }
    g_unlink(remote->path);

    // Quit after the cancelled reads have returned and closed their sockets
    GSource *quit = g_idle_source_new();
    g_source_set_priority(quit, G_PRIORITY_LOW);
    g_source_set_callback(quit, quit_loop, remote->loop, NULL);
    g_source_attach(quit, remote->context);
    g_source_unref(quit);
    return G_SOURCE_REMOVE;
}

void
photon_remote_free(PhotonRemote *remote) {
    if (remote == NULL) return;
    g_main_context_invoke(remote->context, server_stop, remote);
    g_thread_join(remote->thread);

    g_main_loop_unref(remote->loop);
    g_main_context_unref(remote->context);
    g_mutex_clear(&remote->lock);
    g_cond_clear(&remote->started);
    g_free(remote->path);
    g_free(remote);
}
