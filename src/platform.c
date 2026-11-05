/**
 * @author: Ahmad Odeh
 * @date: 5th Nov, 2026
 *
**/
/*
 * platform: OS-specific code (macOS and Linux). See platform.h.
 */
#include "platform.h"

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
