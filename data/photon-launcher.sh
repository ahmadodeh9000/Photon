#!/bin/sh
# @author: Ahmad Odeh
#
# Photon.app/Contents/MacOS/Photon: what Finder, the Dock and Launchpad run.
# They start apps with a bare PATH and "/" as the folder, so give Photon the
# PATH from the user's login shell (Homebrew, lunar, python3) and start it in
# the home folder. Then hand over to the real program next to this script.

here="$(cd "$(dirname "$0")" && pwd)"

# shellcheck disable=SC2016  # $PATH must expand in the login shell, not here
login_path="$("${SHELL:-/bin/zsh}" -l -c 'printf %s "$PATH"' 2>/dev/null)"
if [ -n "$login_path" ]; then
    PATH="$login_path"
    export PATH
fi

if [ "$(pwd)" = "/" ]; then
    cd "$HOME" || exit 1
fi

# Old macOS versions pass a -psn_... process id: drop it
for arg in "$@"; do
    shift
    case "$arg" in -psn_*) ;; *) set -- "$@" "$arg" ;; esac
done

# --detached: this IS the window (output to the log), not the `photon`
# command's launcher, which would hand off to Photon.app again
exec "$here/photon-bin" --detached "$@"
