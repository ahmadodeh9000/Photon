/**
 * @author: Ahmad Odeh
 * @date: 5th Nov, 2026
 *
**/
/*
 * test_editing: auto-close pairs and comment toggling on a real
 * GtkSourceView, driven by synthesized key presses. Needs a display
 * (Linux CI: xvfb-run).
 */
#include <gtk/gtk.h>

#include "../src/document.h"
#include "../src/editing.h"
#include "../src/languages.h"
#include "../src/theme.h"
#include "check.h"

static GtkTextBuffer *
buffer_of(PhotonDocument *doc) {
    return GTK_TEXT_BUFFER(photon_document_get_buffer(doc));
}

static gchar *
text_of(PhotonDocument *doc) {
    GtkTextIter start, end;
    gtk_text_buffer_get_bounds(buffer_of(doc), &start, &end);
    return gtk_text_buffer_get_text(buffer_of(doc), &start, &end, FALSE);
}

static gint
cursor_of(PhotonDocument *doc) {
    GtkTextIter iter;
    gtk_text_buffer_get_iter_at_mark(buffer_of(doc), &iter, gtk_text_buffer_get_insert(buffer_of(doc)));
    return gtk_text_iter_get_offset(&iter);
}

static void
set_text(PhotonDocument *doc, const gchar *text, gint cursor) {
    GtkTextIter iter;
    gtk_text_buffer_set_text(buffer_of(doc), text, -1);
    gtk_text_buffer_get_iter_at_offset(buffer_of(doc), &iter, cursor);
    gtk_text_buffer_place_cursor(buffer_of(doc), &iter);
}

// Connected after Photon's handler, so it only runs for keys Photon left
// alone. It stops them there: the view isn't in a window, so GTK's own
// key handling can't run.
static gboolean fell_through;

static gboolean
catch_unhandled(GtkWidget *view G_GNUC_UNUSED, GdkEventKey *event G_GNUC_UNUSED, gpointer data G_GNUC_UNUSED) {
    fell_through = TRUE;
    return TRUE;
}

static PhotonDocument *
new_doc(const gchar *path) {
    PhotonDocument *doc = photon_document_new();
    photon_document_set_path(doc, path);
    g_signal_connect(photon_document_get_view(doc), "key-press-event", G_CALLBACK(catch_unhandled), NULL);
    return doc;
}

// Returns TRUE if Photon handled the key (FALSE = GTK would insert it as is)
static gboolean
press(PhotonDocument *doc, guint keyval) {
    GdkEvent *event = gdk_event_new(GDK_KEY_PRESS);
    event->key.keyval = keyval;
    event->key.state = 0;
    gboolean handled = FALSE;
    fell_through = FALSE;
    g_signal_emit_by_name(photon_document_get_view(doc), "key-press-event", event, &handled);
    gdk_event_free(event);
    return handled && !fell_through;
}

static void
check_text(PhotonDocument *doc, const gchar *want, gint want_cursor, int line) {
    gchar *got = text_of(doc);
    if (g_strcmp0(got, want) != 0 || cursor_of(doc) != want_cursor) {
        check_failures++;
        g_printerr("line %d: got \"%s\" (cursor %d), want \"%s\" (cursor %d)\n",
                   line, got, cursor_of(doc), want, want_cursor);
    }
    check_count++;
    g_free(got);
}
#define CHECK_TEXT(doc, want, cursor) check_text(doc, want, cursor, __LINE__)

static void
test_pairs(void) {
    PhotonDocument *doc = new_doc("/nowhere/x.lunar");

    set_text(doc, "", 0);
    CHECK(press(doc, GDK_KEY_parenleft));
    CHECK_TEXT(doc, "()", 1);
    CHECK(press(doc, GDK_KEY_parenright));          // steps over
    CHECK_TEXT(doc, "()", 2);

    set_text(doc, "print ", 6);
    CHECK(press(doc, GDK_KEY_quotedbl));
    CHECK_TEXT(doc, "print \"\"", 7);
    CHECK(press(doc, GDK_KEY_BackSpace));           // deletes both
    CHECK_TEXT(doc, "print ", 6);

    set_text(doc, "abc", 3);                        // quote right after a word: no pair
    CHECK(!press(doc, GDK_KEY_quotedbl));

    set_text(doc, "x foo", 2);                      // bracket before a word: no pair
    CHECK(!press(doc, GDK_KEY_bracketleft));

    set_text(doc, "a + b", 0);                      // wrap a selection
    GtkTextIter start, end;
    gtk_text_buffer_get_iter_at_offset(buffer_of(doc), &start, 0);
    gtk_text_buffer_get_iter_at_offset(buffer_of(doc), &end, 5);
    gtk_text_buffer_select_range(buffer_of(doc), &start, &end);
    CHECK(press(doc, GDK_KEY_parenleft));
    gchar *text = text_of(doc);
    CHECK_STR(text, "(a + b)");
    g_free(text);

    set_text(doc, "", 0);                           // Lunar has no '' pair
    CHECK(!press(doc, GDK_KEY_apostrophe));
    photon_document_set_path(doc, "/nowhere/x.py"); // Python does
    set_text(doc, "", 0);
    CHECK(press(doc, GDK_KEY_apostrophe));
    CHECK_TEXT(doc, "''", 1);

    photon_document_free(doc);
}

static void
select_all(PhotonDocument *doc) {
    GtkTextIter start, end;
    gtk_text_buffer_get_bounds(buffer_of(doc), &start, &end);
    gtk_text_buffer_select_range(buffer_of(doc), &start, &end);
}

static void
test_comments(void) {
    PhotonDocument *doc = new_doc("/nowhere/x.lunar");

    set_text(doc, "let a = 1;\n    print a;\n\n", 0);
    select_all(doc);
    photon_editing_toggle_comment(doc);
    gchar *text = text_of(doc);
    CHECK_STR(text, "// let a = 1;\n//     print a;\n\n");
    g_free(text);

    select_all(doc);
    photon_editing_toggle_comment(doc);              // round trip
    text = text_of(doc);
    CHECK_STR(text, "let a = 1;\n    print a;\n\n");
    g_free(text);

    set_text(doc, "    if (x) {\n        y();\n", 0);   // cursor line only, keeps indent
    photon_editing_toggle_comment(doc);
    text = text_of(doc);
    CHECK_STR(text, "    // if (x) {\n        y();\n");
    g_free(text);

    photon_document_set_path(doc, "/nowhere/x.py");
    set_text(doc, "x = 1\n", 0);
    photon_editing_toggle_comment(doc);
    text = text_of(doc);
    CHECK_STR(text, "# x = 1\n");
    g_free(text);

    photon_document_free(doc);
}

static guint
marks_on_line(PhotonDocument *doc, gint line) {
    GSList *marks = gtk_source_buffer_get_source_marks_at_line(photon_document_get_buffer(doc), line, NULL);
    guint n = g_slist_length(marks);
    g_slist_free(marks);
    return n;
}

static void
test_markers(void) {
    PhotonDocument *doc = new_doc("/nowhere/m.lunar");
    set_text(doc, "let a = 1;\nprint b;\n", 0);

    GPtrArray *errors = photon_errors_parse_lunar(
        "Undefined variable 'b'.\n[line 2] in script\n"
        "Other file.\n[lib/x.lunar line 1] in script\n", "/nowhere/m.lunar", "/nowhere");
    photon_document_show_errors(doc, errors);
    CHECK(marks_on_line(doc, 1) == 1);     // 0-based: line 2 of the file
    CHECK(marks_on_line(doc, 0) == 0);     // lib/x.lunar's error isn't ours

    photon_document_goto(doc, 2, 0);
    CHECK(cursor_of(doc) == 11);

    gtk_text_buffer_insert_at_cursor(buffer_of(doc), " ", -1);   // any edit clears them
    CHECK(marks_on_line(doc, 1) == 0);

    g_ptr_array_unref(errors);
    photon_document_free(doc);
}

static void
test_registry(void) {
    CHECK_STR(photon_languages_for_file("a/b/Game.LUNAR")->id, "lunar");
    CHECK_STR(photon_languages_for_file("x.lun")->id, "lunar");
    CHECK_STR(photon_languages_for_file("x.h")->id, "c");
    CHECK_STR(photon_languages_for_file("x.py")->gsv_lang, "python3");
    CHECK(photon_languages_for_file("Makefile") == NULL);
    CHECK(photon_languages_get("lunar") != NULL);    // our lang/ is on the search path
    CHECK(photon_languages_get("python3") != NULL);
    CHECK(photon_languages_get("c") != NULL);
}

int
main(int argc, char *argv[]) {
    gtk_init(&argc, &argv);
    photon_languages_init(".");    // run from the repo root
    photon_theme_init(".");

    test_registry();
    test_pairs();
    test_comments();
    test_markers();
    return check_finish("test_editing");
}
