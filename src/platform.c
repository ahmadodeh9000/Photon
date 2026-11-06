/**
 * @author: Ahmad Odeh
 * @date: 5th Nov, 2026
 *
**/
/*
 * platform: OS-specific code (macOS and Linux). See platform.h.
 */
#include "platform.h"

#include <fcntl.h>
#include <glib/gstdio.h>
#include <unistd.h>

#ifdef __APPLE__
#include <limits.h>
#include <mach-o/dyld.h>
#include <stdint.h>
#include <sys/resource.h>
#endif

#ifndef PHOTON_DATADIR
#define PHOTON_DATADIR "/usr/local/share/photon"
#endif

void
photon_platform_init(void) {
#ifdef __APPLE__
    // VTE's fdwalk() on macOS sizes a stack buffer by the open-files soft
    // limit. Started from a shell with a huge `ulimit -n` (1048576 is
    // common), every terminal spawn segfaults in the child. macOS's own
    // per-process maximum is OPEN_MAX (10240), so cap the soft limit there.
    struct rlimit limit;
    if (getrlimit(RLIMIT_NOFILE, &limit) == 0 && limit.rlim_cur > OPEN_MAX) {
        limit.rlim_cur = OPEN_MAX;
        setrlimit(RLIMIT_NOFILE, &limit);
    }
#endif
}

static gchar *
read_executable_path(void) {
#if defined(__APPLE__)
    uint32_t size = 0;
    _NSGetExecutablePath(NULL, &size);          // asks for the needed size
    gchar *buf = g_malloc(size + 1);
    if (_NSGetExecutablePath(buf, &size) != 0) {
        g_free(buf);
        return NULL;
    }
    gchar *real = g_canonicalize_filename(buf, NULL);
    g_free(buf);
    return real;
#elif defined(__linux__)
    return g_file_read_link("/proc/self/exe", NULL);
#else
    return NULL;
#endif
}

gchar *
photon_platform_executable_path(void) {
    return read_executable_path();
}

gchar *
photon_platform_executable_dir(void) {
    gchar *exe = read_executable_path();
    if (exe == NULL) return NULL;

    gchar *dir = g_path_get_dirname(exe);
    g_free(exe);
    return dir;
}

// A data dir is valid if it has our Lunar language spec in it
static gboolean
is_data_dir(const gchar *dir) {
    gchar *probe = g_build_filename(dir, "lang", "lunar.lang", NULL);
    gboolean ok = g_file_test(probe, G_FILE_TEST_IS_REGULAR);
    g_free(probe);
    return ok;
}

gchar *
photon_platform_data_dir(void) {
    const gchar *env = g_getenv("PHOTON_DATA_DIR");
    if (env != NULL && is_data_dir(env)) return g_strdup(env);

    gchar *exe_dir = photon_platform_executable_dir();
    if (exe_dir != NULL) {
        gchar *candidates[] = {
            g_strdup(exe_dir),                                        // ./Photon in the repo
            g_build_filename(exe_dir, "..", NULL),                    // build/ inside the repo
            g_build_filename(exe_dir, "..", "share", "photon", NULL), // <prefix>/bin/photon
#ifdef __APPLE__
            g_build_filename(exe_dir, "..", "Resources", NULL),       // Photon.app/Contents/MacOS
#endif
            NULL
        };
        gchar *found = NULL;
        for (int i = 0; candidates[i] != NULL; i++) {
            if (found == NULL && is_data_dir(candidates[i])) {
                found = g_canonicalize_filename(candidates[i], NULL);
            }
            g_free(candidates[i]);
        }
        g_free(exe_dir);
        if (found != NULL) return found;
    }

    if (is_data_dir(PHOTON_DATADIR)) return g_strdup(PHOTON_DATADIR);
    return NULL;
}

// ---------------------------------------------------------------------
// Detached start
// ---------------------------------------------------------------------

// Child side of the spawn: a new session, so closing the terminal (SIGHUP
// to its session) doesn't reach Photon
static void
new_session(gpointer user_data G_GNUC_UNUSED) {
    setsid();
}

#define SPAWN_QUIET (G_SPAWN_STDIN_FROM_DEV_NULL | G_SPAWN_STDOUT_TO_DEV_NULL | G_SPAWN_STDERR_TO_DEV_NULL)

#ifdef __APPLE__
// Photon.app that holds this executable (…/Photon.app/Contents/MacOS/x), or NULL
static gchar *
app_bundle(const gchar *exe) {
    gchar *macos = g_path_get_dirname(exe);
    gchar *contents = g_path_get_dirname(macos);
    gchar *bundle = g_path_get_dirname(contents);
    gchar *macos_name = g_path_get_basename(macos);
    gboolean inside = g_str_equal(macos_name, "MacOS") && g_str_has_suffix(bundle, ".app");
    g_free(macos_name);
    g_free(contents);
    g_free(macos);
    if (inside) return bundle;
    g_free(bundle);
    return NULL;
}

// open -a Photon.app --args ARGS: LaunchServices starts the bundle's launcher
static gboolean
start_bundle(const gchar *bundle, gchar **args, GError **error) {
    GPtrArray *argv = g_ptr_array_new();
    g_ptr_array_add(argv, "/usr/bin/open");
    g_ptr_array_add(argv, "-a");
    g_ptr_array_add(argv, (gpointer)bundle);
    g_ptr_array_add(argv, "--args");
    for (gint i = 0; args != NULL && args[i] != NULL; i++) g_ptr_array_add(argv, args[i]);
    g_ptr_array_add(argv, NULL);

    gboolean ok = g_spawn_async(NULL, (gchar **)argv->pdata, NULL, SPAWN_QUIET, NULL, NULL, NULL, error);
    g_ptr_array_free(argv, TRUE);
    return ok;
}
#endif

gboolean
photon_platform_start_detached(gchar **args, GError **error) {
    gchar *exe = read_executable_path();
    if (exe == NULL) {
        g_set_error_literal(error, G_FILE_ERROR, G_FILE_ERROR_NOENT, "can't find the Photon program");
        return FALSE;
    }

#ifdef __APPLE__
    gchar *bundle = app_bundle(exe);
    if (bundle != NULL) {
        gboolean ok = start_bundle(bundle, args, error);
        g_free(bundle);
        g_free(exe);
        return ok;
    }
#endif

    GPtrArray *argv = g_ptr_array_new();
    g_ptr_array_add(argv, exe);
    for (gint i = 0; args != NULL && args[i] != NULL; i++) g_ptr_array_add(argv, args[i]);
    g_ptr_array_add(argv, NULL);

    // Without G_SPAWN_DO_NOT_REAP_CHILD GLib forks twice: nobody has to wait for Photon
    gboolean ok = g_spawn_async(NULL, (gchar **)argv->pdata, NULL, SPAWN_QUIET, new_session, NULL, NULL, error);
    g_ptr_array_free(argv, TRUE);
    g_free(exe);
    return ok;
}

gchar *
photon_platform_runtime_dir(void) {
    const gchar *override = g_getenv("PHOTON_RUNTIME_DIR");
    gchar *dir;
    if (override != NULL && *override != '\0') {
        dir = g_strdup(override);
    } else {
#ifdef __APPLE__
        dir = g_build_filename(g_get_home_dir(), "Library", "Caches", "photon", NULL);
#else
        dir = g_build_filename(g_get_user_runtime_dir(), "photon", NULL);
#endif
    }
    g_mkdir_with_parents(dir, 0700);
    return dir;
}

#define LOG_MAX_BYTES (1024 * 1024)

void
photon_platform_redirect_output(const gchar *log_path) {
    gchar *dir = g_path_get_dirname(log_path);
    g_mkdir_with_parents(dir, 0700);
    g_free(dir);

    GStatBuf st;
    if (g_stat(log_path, &st) == 0 && st.st_size > LOG_MAX_BYTES) {
        gchar *old = g_strconcat(log_path, ".1", NULL);
        g_rename(log_path, old);
        g_free(old);
    }

    int null_fd = open("/dev/null", O_RDONLY);
    if (null_fd >= 0) {
        dup2(null_fd, STDIN_FILENO);
        close(null_fd);
    }
    int log_fd = open(log_path, O_WRONLY | O_CREAT | O_APPEND, 0600);
    if (log_fd >= 0) {
        dup2(log_fd, STDOUT_FILENO);
        dup2(log_fd, STDERR_FILENO);
        close(log_fd);
    }
}

gboolean
photon_platform_primary_is_control(void) {
#ifdef __APPLE__
    return FALSE;
#else
    return TRUE;
#endif
}

const gchar *
photon_platform_fallback_shell(void) {
#ifdef __APPLE__
    return "/bin/zsh";   // default login shell since macOS 10.15
#else
    return "/bin/sh";    // always present on Linux; bash may not be
#endif
}
