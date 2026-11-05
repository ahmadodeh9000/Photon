/**
 * @author: Ahmad Odeh
 * @date: 5th Nov, 2026
 *
**/
/*
 * check: the smallest test helper that works. CHECK(cond) and
 * CHECK_STR(a, b) report the failing line and keep going; finish() returns
 * the exit code for main().
 */
#ifndef PHOTON_CHECK_H
#define PHOTON_CHECK_H

#include <glib.h>

static int check_failures = 0;
static int check_count = 0;

#define CHECK(cond) do { \
    check_count++; \
    if (!(cond)) { check_failures++; g_printerr("%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #cond); } \
} while (0)

#define CHECK_STR(got, want) do { \
    const gchar *g_ = (got), *w_ = (want); \
    check_count++; \
    if (g_strcmp0(g_, w_) != 0) { \
        check_failures++; \
        g_printerr("%s:%d: got \"%s\", want \"%s\"\n", __FILE__, __LINE__, g_ ? g_ : "(null)", w_ ? w_ : "(null)"); \
    } \
} while (0)

static inline int
check_finish(const gchar *name) {
    g_print("%s: %d checks, %d failed\n", name, check_count, check_failures);
    return check_failures == 0 ? 0 : 1;
}

#endif
