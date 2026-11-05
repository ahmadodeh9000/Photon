/**
 * @author: Ahmad Odeh
 * @date: 5th Nov, 2026
 *
**/
/*
 * test_errors: the stderr parsers, fed with real output captured from
 * lunar, clang, gcc and python3.
 */
#include <glib/gstdio.h>
#include <unistd.h>

#include "../src/errors.h"
#include "check.h"

#define DIR "/work"

static const PhotonError *
at(GPtrArray *list, guint i) {
    return i < list->len ? g_ptr_array_index(list, i) : NULL;
}

static void
test_lunar(void) {
    // compile error, exit 110: every error line counts
    GPtrArray *list = photon_errors_parse_lunar(
        "[line 1] ERROR at ';': Expected expression.\n"
        "[line 2] ERROR: Unterminated string.\n", DIR "/main.lunar", DIR);
    CHECK(list->len == 2);
    CHECK_STR(at(list, 0)->path, DIR "/main.lunar");
    CHECK(at(list, 0)->line == 1);
    CHECK_STR(at(list, 0)->message, "Expected expression. (at ';')");
    CHECK(at(list, 1)->line == 2);
    CHECK_STR(at(list, 1)->message, "Unterminated string.");
    g_ptr_array_unref(list);

    // runtime error, exit 100: message, then the innermost frame
    list = photon_errors_parse_lunar(
        "Array index 10 out of bounds (length 2).\n"
        "[lib/errfn.lunar line 3] in helper()\n"
        "[lib/errfn.lunar line 5] in boom()\n"
        "[line 4] in script\n", DIR "/main.lunar", DIR);
    CHECK(list->len == 1);
    CHECK_STR(at(list, 0)->path, DIR "/lib/errfn.lunar");
    CHECK(at(list, 0)->line == 3);
    CHECK_STR(at(list, 0)->message, "Array index 10 out of bounds (length 2).");
    g_ptr_array_unref(list);

    // a module that doesn't compile: its compile error comes first
    list = photon_errors_parse_lunar(
        "[lib/util.lunar line 4] ERROR at '}': Expect ';' after return value.\n"
        "Can't compile module 'lib/util.lunar'.\n"
        "[line 1] in script\n", DIR "/main.lunar", DIR);
    CHECK(list->len == 2);
    CHECK_STR(at(list, 0)->path, DIR "/lib/util.lunar");
    CHECK(at(list, 0)->line == 4);
    CHECK_STR(at(list, 1)->path, DIR "/main.lunar");
    CHECK_STR(at(list, 1)->message, "Can't compile module 'lib/util.lunar'.");
    g_ptr_array_unref(list);

    // collapsed stack overflow trace
    list = photon_errors_parse_lunar(
        "Stack overflow.\n[line 1] in f()\n[line 1] in f()\n"
        "... repeated 1020 more times\n[line 2] in script\n", DIR "/main.lunar", DIR);
    CHECK(list->len == 1);
    CHECK(at(list, 0)->line == 1);
    g_ptr_array_unref(list);

    // clean run
    list = photon_errors_parse_lunar("", DIR "/main.lunar", DIR);
    CHECK(list->len == 0);
    g_ptr_array_unref(list);
}

static void
test_c(void) {
    // clang (macOS)
    GPtrArray *list = photon_errors_parse_c(
        "/work/bad.c:5:20: error: use of undeclared identifier 'missing'\n"
        "    5 |     printf(\"%d\\n\", missing);\n"
        "      |                    ^\n"
        "1 error generated.\n", DIR "/bad.c", DIR);
    CHECK(list->len == 1);
    CHECK_STR(at(list, 0)->path, DIR "/bad.c");
    CHECK(at(list, 0)->line == 5);
    CHECK(at(list, 0)->column == 20);
    CHECK(!at(list, 0)->is_warning);
    g_ptr_array_unref(list);

    // gcc (Linux): relative path, "In function" noise, notes, warnings
    list = photon_errors_parse_c(
        "bad.c: In function 'main':\n"
        "bad.c:4:9: warning: unused variable 'unused' [-Wunused-variable]\n"
        "bad.c:5:20: error: 'missing' undeclared (first use in this function)\n"
        "bad.c:5:20: note: each undeclared identifier is reported only once\n"
        "src/x.h:12: error: no column here\n", DIR "/bad.c", DIR);
    CHECK(list->len == 3);
    CHECK(at(list, 0)->is_warning);
    CHECK(at(list, 0)->line == 4);
    CHECK_STR(at(list, 1)->path, DIR "/bad.c");
    CHECK(at(list, 1)->column == 20);
    CHECK_STR(at(list, 2)->path, DIR "/src/x.h");
    CHECK(at(list, 2)->column == 0);
    CHECK(photon_errors_first(list) == at(list, 1));   // first ERROR, not the warning
    g_ptr_array_unref(list);

    // linker errors have no location
    list = photon_errors_parse_c(
        "Undefined symbols for architecture arm64:\n"
        "clang: error: linker command failed with exit code 1\n", DIR "/a.c", DIR);
    CHECK(list->len == 0);
    g_ptr_array_unref(list);
}

static void
test_python(void) {
    // The parser only keeps frames whose file exists
    gchar *path = NULL;
    gint fd = g_file_open_tmp("photon-test-XXXXXX.py", &path, NULL);
    CHECK(fd >= 0);
    close(fd);

    gchar *traceback = g_strdup_printf(
        "Your name? Traceback (most recent call last):\n"
        "  File \"%s\", line 5, in <module>\n"
        "    f(1)\n"
        "  File \"<frozen runpy>\", line 88, in _run_code\n"
        "  File \"%s\", line 2, in f\n"
        "    return x / 0\n"
        "           ~~^~~\n"
        "ZeroDivisionError: division by zero\n", path, path);
    GPtrArray *list = photon_errors_parse_python(traceback, path, "/");
    CHECK(list->len == 1);
    CHECK_STR(at(list, 0)->path, path);
    CHECK(at(list, 0)->line == 2);
    CHECK_STR(at(list, 0)->message, "ZeroDivisionError: division by zero");
    g_ptr_array_unref(list);
    g_free(traceback);

    gchar *syntax = g_strdup_printf(
        "  File \"%s\", line 1\n    def f(:\n          ^\nSyntaxError: invalid syntax\n", path);
    list = photon_errors_parse_python(syntax, path, "/");
    CHECK(list->len == 1);
    CHECK(at(list, 0)->line == 1);
    CHECK_STR(at(list, 0)->message, "SyntaxError: invalid syntax");
    g_ptr_array_unref(list);
    g_free(syntax);

    g_unlink(path);
    g_free(path);
}

int
main(void) {
    test_lunar();
    test_c();
    test_python();
    return check_finish("test_errors");
}
