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
#include <mach-o/dyld.h>
#include <stdint.h>
#endif

#ifndef PHOTON_DATADIR
#define PHOTON_DATADIR "/usr/local/share/photon"
#endif

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

const gchar *
photon_platform_fallback_shell(void) {
#ifdef __APPLE__
    return "/bin/zsh";   // default login shell since macOS 10.15
#else
    return "/bin/sh";    // always present on Linux; bash may not be
#endif
}
