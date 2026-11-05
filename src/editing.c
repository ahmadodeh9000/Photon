/**
 * @author: Ahmad Odeh
 * @date: 5th Nov, 2026
 *
**/
/*
 * editing: auto-close pairs and comment toggling. See editing.h.
 */
#include "editing.h"

#include <string.h>

// ---------------------------------------------------------------------
// Auto-close pairs
//   typing an opener inserts the closer, or wraps the selection
//   typing a closer that's already next to the cursor just steps over it
//   Backspace between an empty pair deletes both
// ---------------------------------------------------------------------

// Closer for `c` if it opens a pair in this language, else 0
static gunichar
closer_for(const PhotonLanguage *lang, gunichar c) {
    for (const gchar *p = lang->pairs; p[0] != '\0' && p[1] != '\0'; p += 2) {
        if ((gunichar)p[0] == c) return (gunichar)p[1];
    }
    return 0;
}

static gboolean
is_closer(const PhotonLanguage *lang, gunichar c) {
    for (const gchar *p = lang->pairs; p[0] != '\0' && p[1] != '\0'; p += 2) {
        if ((gunichar)p[1] == c) return TRUE;
    }
    return FALSE;
}

static gboolean
is_word_char(gunichar c) {
    return g_unichar_isalnum(c) || c == '_';
}

static gunichar
char_before(const GtkTextIter *iter) {
    GtkTextIter prev = *iter;
    return gtk_text_iter_backward_char(&prev) ? gtk_text_iter_get_char(&prev) : 0;
}

// Only auto-close where it can't be in the middle of a word ("don't", x[i)
static gboolean
should_close(gunichar open, gunichar close, const GtkTextIter *cursor) {
    gunichar next = gtk_text_iter_get_char(cursor);
    gunichar prev = char_before(cursor);

    if (is_word_char(next)) return FALSE;
    if (open == close) {                       // quotes
        if (is_word_char(prev) || prev == '\\') return FALSE;
    }
    return TRUE;
}

static void
insert_pair(GtkTextBuffer *buffer, gunichar open, gunichar close) {
    GtkTextIter start, end;
    gchar open_s[8] = { 0 }, close_s[8] = { 0 };
    g_unichar_to_utf8(open, open_s);
    g_unichar_to_utf8(close, close_s);

    gtk_text_buffer_begin_user_action(buffer);
    if (gtk_text_buffer_get_selection_bounds(buffer, &start, &end)) {
        // Wrap the selection, keeping it selected inside the pair
        GtkTextMark *end_mark = gtk_text_buffer_create_mark(buffer, NULL, &end, FALSE);
        gtk_text_buffer_insert(buffer, &start, open_s, -1);
        GtkTextMark *start_mark = gtk_text_buffer_create_mark(buffer, NULL, &start, TRUE);
        gtk_text_buffer_get_iter_at_mark(buffer, &end, end_mark);
        gtk_text_buffer_insert(buffer, &end, close_s, -1);

        gtk_text_buffer_get_iter_at_mark(buffer, &start, start_mark);
        gtk_text_buffer_get_iter_at_mark(buffer, &end, end_mark);
        gtk_text_iter_backward_char(&end);
        gtk_text_buffer_select_range(buffer, &start, &end);
        gtk_text_buffer_delete_mark(buffer, start_mark);
        gtk_text_buffer_delete_mark(buffer, end_mark);
    } else {
        gchar both[16];
        g_snprintf(both, sizeof both, "%s%s", open_s, close_s);
        gtk_text_buffer_insert_at_cursor(buffer, both, -1);
        gtk_text_buffer_get_iter_at_mark(buffer, &start, gtk_text_buffer_get_insert(buffer));
        gtk_text_iter_backward_char(&start);
        gtk_text_buffer_place_cursor(buffer, &start);
    }
    gtk_text_buffer_end_user_action(buffer);
}

static gboolean
handle_backspace(const PhotonLanguage *lang, GtkTextBuffer *buffer) {
    GtkTextIter cursor, prev, next;
    if (gtk_text_buffer_get_has_selection(buffer)) return FALSE;

    gtk_text_buffer_get_iter_at_mark(buffer, &cursor, gtk_text_buffer_get_insert(buffer));
    gunichar before = char_before(&cursor);
    gunichar after = gtk_text_iter_get_char(&cursor);
    if (before == 0 || closer_for(lang, before) != after) return FALSE;

    prev = cursor;
    next = cursor;
    gtk_text_iter_backward_char(&prev);
    gtk_text_iter_forward_char(&next);
    gtk_text_buffer_begin_user_action(buffer);
    gtk_text_buffer_delete(buffer, &prev, &next);
    gtk_text_buffer_end_user_action(buffer);
    return TRUE;
}

// ---------------------------------------------------------------------
// Indentation helpers
// ---------------------------------------------------------------------

// Leading whitespace of the cursor's line, up to the cursor
static gchar *
line_indent(GtkTextBuffer *buffer, const GtkTextIter *cursor) {
    GtkTextIter start = *cursor, end;
    gtk_text_iter_set_line_offset(&start, 0);
    end = start;
    while (gtk_text_iter_compare(&end, cursor) < 0 && (gtk_text_iter_get_char(&end) == ' ' || gtk_text_iter_get_char(&end) == '\t')) {
        gtk_text_iter_forward_char(&end);
    }
    return gtk_text_buffer_get_text(buffer, &start, &end, FALSE);
}

// Is everything between the line start and the cursor blank?
static gboolean
only_blanks_before(const GtkTextIter *cursor) {
    GtkTextIter iter = *cursor;
    gtk_text_iter_set_line_offset(&iter, 0);
    for (; gtk_text_iter_compare(&iter, cursor) < 0; gtk_text_iter_forward_char(&iter)) {
        gunichar c = gtk_text_iter_get_char(&iter);
        if (c != ' ' && c != '\t') return FALSE;
    }
    return TRUE;
}

// Last non-blank char before the cursor on the same line, or 0
static gunichar
last_char_before(const GtkTextIter *cursor) {
    GtkTextIter iter = *cursor;
    while (!gtk_text_iter_starts_line(&iter) && gtk_text_iter_backward_char(&iter)) {
        gunichar c = gtk_text_iter_get_char(&iter);
        if (c != ' ' && c != '\t') return c;
    }
    return 0;
}

// Enter after "{" (or ":" in Python) indents one level; Enter between "{}"
// opens a block:   {|}   ->   {
//                                 |
//                             }
static gboolean
handle_return(const PhotonLanguage *lang, GtkTextBuffer *buffer) {
    GtkTextIter cursor;
    if (lang->indent_after == NULL || gtk_text_buffer_get_has_selection(buffer)) return FALSE;
    gtk_text_buffer_get_iter_at_mark(buffer, &cursor, gtk_text_buffer_get_insert(buffer));

    gunichar opener = last_char_before(&cursor);
    if (opener == 0 || strchr(lang->indent_after, (int)opener) == NULL) return FALSE;

    gchar *indent = line_indent(buffer, &cursor);
    gchar *unit = g_strnfill(lang->indent, ' ');
    gboolean closes = closer_for(lang, opener) != 0 && gtk_text_iter_get_char(&cursor) == closer_for(lang, opener);

    gchar *first = g_strconcat("\n", indent, unit, NULL);
    gtk_text_buffer_begin_user_action(buffer);
    gtk_text_buffer_insert(buffer, &cursor, first, -1);
    if (closes) {
        GtkTextMark *here = gtk_text_buffer_create_mark(buffer, NULL, &cursor, TRUE);
        gchar *second = g_strconcat("\n", indent, NULL);
        gtk_text_buffer_insert(buffer, &cursor, second, -1);
        gtk_text_buffer_get_iter_at_mark(buffer, &cursor, here);
        gtk_text_buffer_delete_mark(buffer, here);
        g_free(second);
    }
    gtk_text_buffer_place_cursor(buffer, &cursor);
    gtk_text_buffer_end_user_action(buffer);

    g_free(first);
    g_free(unit);
    g_free(indent);
    return TRUE;
}

// Typing "}" on a blank line removes one indent level first
static void
dedent_for_closer(const PhotonLanguage *lang, GtkTextBuffer *buffer, gunichar c) {
    GtkTextIter cursor, start;
    if (lang->indent_after == NULL) return;

    gboolean closes_indent = FALSE;
    for (const gchar *o = lang->indent_after; *o != '\0'; o++) {
        if (closer_for(lang, (gunichar)*o) == c) closes_indent = TRUE;
    }
    gtk_text_buffer_get_iter_at_mark(buffer, &cursor, gtk_text_buffer_get_insert(buffer));
    if (!closes_indent || !only_blanks_before(&cursor)) return;

    gint column = gtk_text_iter_get_line_offset(&cursor);
    gint remove = MIN(column, (gint)lang->indent);
    if (remove == 0) return;
    start = cursor;
    gtk_text_iter_backward_chars(&start, remove);
    gtk_text_buffer_delete(buffer, &start, &cursor);
}

static gboolean
handle_char(const PhotonLanguage *lang, GtkTextBuffer *buffer, gunichar c) {
    GtkTextIter cursor;
    gboolean has_selection = gtk_text_buffer_get_has_selection(buffer);
    gtk_text_buffer_get_iter_at_mark(buffer, &cursor, gtk_text_buffer_get_insert(buffer));

    // Step over a closer that is already there
    if (!has_selection && is_closer(lang, c) && gtk_text_iter_get_char(&cursor) == c) {
        gtk_text_iter_forward_char(&cursor);
        gtk_text_buffer_place_cursor(buffer, &cursor);
        return TRUE;
    }

    // "}" typed on a blank line: dedent, then let GTK insert it
    if (!has_selection && is_closer(lang, c) && closer_for(lang, c) != c) {
        gtk_text_buffer_begin_user_action(buffer);
        dedent_for_closer(lang, buffer, c);
        gtk_text_buffer_end_user_action(buffer);
        return FALSE;
    }

    gunichar close = closer_for(lang, c);
    if (close == 0) return FALSE;
    if (!has_selection && !should_close(c, close, &cursor)) return FALSE;

    insert_pair(buffer, c, close);
    return TRUE;
}

static gboolean
on_key_press(GtkWidget *view, GdkEventKey *event, gpointer user_data) {
    PhotonDocument *doc = user_data;
    const PhotonLanguage *lang = photon_document_get_language(doc);
    if (lang == NULL || lang->pairs == NULL) return FALSE;

    // Leave shortcuts (Ctrl/Cmd/Alt + key) alone; Shift is fine
    GdkModifierType mods = event->state & gtk_accelerator_get_default_mod_mask() & ~GDK_SHIFT_MASK;
    if (mods != 0) return FALSE;

    GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(view));
    if (event->keyval == GDK_KEY_BackSpace) return handle_backspace(lang, buffer);
    if (event->keyval == GDK_KEY_Return || event->keyval == GDK_KEY_KP_Enter) {
        if ((event->state & GDK_SHIFT_MASK) || !handle_return(lang, buffer)) return FALSE;
        gtk_text_view_scroll_mark_onscreen(GTK_TEXT_VIEW(view), gtk_text_buffer_get_insert(buffer));
        return TRUE;
    }

    gunichar c = gdk_keyval_to_unicode(event->keyval);
    return c != 0 && handle_char(lang, buffer, c);
}

void
photon_editing_attach(PhotonDocument *doc) {
    g_signal_connect(photon_document_get_view(doc), "key-press-event", G_CALLBACK(on_key_press), doc);
}

// ---------------------------------------------------------------------
// Comment toggle
// ---------------------------------------------------------------------

// Offset of the first non-blank char on the line, or -1 if it's blank
static gint
indent_of(GtkTextBuffer *buffer, gint line) {
    GtkTextIter iter;
    gtk_text_buffer_get_iter_at_line(buffer, &iter, line);
    while (!gtk_text_iter_ends_line(&iter)) {
        if (!g_unichar_isspace(gtk_text_iter_get_char(&iter))) return gtk_text_iter_get_line_offset(&iter);
        gtk_text_iter_forward_char(&iter);
    }
    return -1;
}

static gboolean
line_is_commented(GtkTextBuffer *buffer, gint line, gint indent, const gchar *marker) {
    GtkTextIter start, end;
    gtk_text_buffer_get_iter_at_line_offset(buffer, &start, line, indent);
    end = start;
    gtk_text_iter_forward_chars(&end, (gint)strlen(marker));
    gchar *text = gtk_text_buffer_get_text(buffer, &start, &end, FALSE);
    gboolean yes = g_strcmp0(text, marker) == 0;
    g_free(text);
    return yes;
}

static void
uncomment_line(GtkTextBuffer *buffer, gint line, gint indent, const gchar *marker) {
    GtkTextIter start, end;
    gtk_text_buffer_get_iter_at_line_offset(buffer, &start, line, indent);
    end = start;
    gtk_text_iter_forward_chars(&end, (gint)strlen(marker));
    if (gtk_text_iter_get_char(&end) == ' ') gtk_text_iter_forward_char(&end);
    gtk_text_buffer_delete(buffer, &start, &end);
}

// Lines covered by the selection; a selection ending at column 0 stops the line before
static void
selected_lines(GtkTextBuffer *buffer, gint *first, gint *last) {
    GtkTextIter start, end;
    gtk_text_buffer_get_selection_bounds(buffer, &start, &end);
    *first = gtk_text_iter_get_line(&start);
    *last = gtk_text_iter_get_line(&end);
    if (*last > *first && gtk_text_iter_starts_line(&end)) (*last)--;
}

void
photon_editing_toggle_comment(PhotonDocument *doc) {
    const PhotonLanguage *lang = photon_document_get_language(doc);
    if (lang == NULL || lang->comment == NULL) return;

    GtkTextBuffer *buffer = GTK_TEXT_BUFFER(photon_document_get_buffer(doc));
    const gchar *marker = lang->comment;
    gint first, last;
    selected_lines(buffer, &first, &last);

    // Comment out unless every non-blank line is already commented
    gboolean all_commented = TRUE;
    gint min_indent = G_MAXINT;
    for (gint line = first; line <= last; line++) {
        gint indent = indent_of(buffer, line);
        if (indent < 0) continue;
        min_indent = MIN(min_indent, indent);
        if (!line_is_commented(buffer, line, indent, marker)) all_commented = FALSE;
    }
    if (min_indent == G_MAXINT) return;   // only blank lines

    gchar *prefix = g_strdup_printf("%s ", marker);
    gtk_text_buffer_begin_user_action(buffer);
    for (gint line = first; line <= last; line++) {
        gint indent = indent_of(buffer, line);
        if (indent < 0) continue;
        if (all_commented) {
            uncomment_line(buffer, line, indent, marker);
        } else {
            GtkTextIter at;
            gtk_text_buffer_get_iter_at_line_offset(buffer, &at, line, min_indent);
            gtk_text_buffer_insert(buffer, &at, prefix, -1);
        }
    }
    gtk_text_buffer_end_user_action(buffer);
    g_free(prefix);
}

// ---------------------------------------------------------------------
// Line commands
// ---------------------------------------------------------------------
void
photon_editing_duplicate_lines(PhotonDocument *doc) {
    GtkTextBuffer *buffer = GTK_TEXT_BUFFER(photon_document_get_buffer(doc));
    GtkTextIter start, end, cursor;
    gint first, last;
    selected_lines(buffer, &first, &last);

    gtk_text_buffer_get_iter_at_line(buffer, &start, first);
    gtk_text_buffer_get_iter_at_line(buffer, &end, last);
    if (!gtk_text_iter_ends_line(&end)) gtk_text_iter_forward_to_line_end(&end);
    gchar *block = gtk_text_buffer_get_text(buffer, &start, &end, FALSE);
    gchar *copy = g_strconcat("\n", block, NULL);

    // Remember where the cursor is in the block, to put it in the copy
    gtk_text_buffer_get_iter_at_mark(buffer, &cursor, gtk_text_buffer_get_insert(buffer));
    gint line = gtk_text_iter_get_line(&cursor) + (last - first + 1);
    gint column = gtk_text_iter_get_line_offset(&cursor);

    gtk_text_buffer_begin_user_action(buffer);
    gtk_text_buffer_insert(buffer, &end, copy, -1);
    gtk_text_buffer_get_iter_at_line_offset(buffer, &cursor, line, column);
    gtk_text_buffer_place_cursor(buffer, &cursor);
    gtk_text_buffer_end_user_action(buffer);

    g_free(copy);
    g_free(block);
}

void
photon_editing_move_lines(PhotonDocument *doc, gboolean down) {
    // GtkSourceView already knows how, keeping the selection and undo
    g_signal_emit_by_name(photon_document_get_view(doc), "move-lines", down);
}
