/**
 * @author: Ahmad Odeh
 * @date: 5th Nov, 2026
 *
**/
/*
 * platform: every bit of OS-specific code in Photon lives here (and only here).
 * The rest of the app asks these functions instead of using #ifdef __APPLE__.
 */
#ifndef PHOTON_PLATFORM_H
#define PHOTON_PLATFORM_H

#include <glib.h>

// Directory holding the running executable, or NULL if it can't be found.
gchar *photon_platform_executable_dir(void);

// Folder with Photon's data files (lang/, styles/, icons/), or NULL.
// Checks $PHOTON_DATA_DIR, the source tree next to the binary, then the
// install prefix.
gchar *photon_platform_data_dir(void);

// Shell to use when VTE can't tell us the user's login shell.
const gchar *photon_platform_fallback_shell(void);

#endif
