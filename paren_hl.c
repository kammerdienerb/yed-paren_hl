#include <yed/plugin.h>

void paren_hl_cursor_moved_handler(yed_event *event);
void paren_hl_buff_mod_handler(yed_event *event);
void paren_hl_line_handler(yed_event *event);

void paren_hl_find_parens(yed_frame *frame);
void paren_hl_hl_parens(yed_event *event);

#define LINE_MAX 500

static int dirty;
static int beg_row;
static int beg_col;
static int end_row;
static int end_col;

int yed_plugin_boot(yed_plugin *self) {
    yed_event_handler cursor_moved, buff_mod, line;

    YED_PLUG_VERSION_CHECK();

    cursor_moved.kind  = EVENT_CURSOR_POST_MOVE;
    cursor_moved.fn    = paren_hl_cursor_moved_handler;
    buff_mod.kind      = EVENT_BUFFER_POST_MOD;
    buff_mod.fn        = paren_hl_buff_mod_handler;
    line.kind          = EVENT_LINE_PRE_DRAW;
    line.fn            = paren_hl_line_handler;

    yed_plugin_add_event_handler(self, cursor_moved);
    yed_plugin_add_event_handler(self, line);
    yed_plugin_add_event_handler(self, buff_mod);

    if (yed_get_var("paren-hl-max-line-length") == NULL) {
        yed_set_var("paren-hl-max-line-length", XSTR(LINE_MAX));
    }

    return 0;
}

void paren_hl_cursor_moved_handler(yed_event *event) {
    yed_frame *frame;

    frame = event->frame;

    if (!frame
    ||  !frame->buffer
    ||  frame->buffer->kind != BUFF_KIND_FILE) {
        return;
    }

    dirty = 1;
}

void paren_hl_buff_mod_handler(yed_event *event) {
    yed_frame *frame;

    frame = event->frame;

    if (!frame
    ||  frame != ys->active_frame
    ||  !frame->buffer
    ||  frame->buffer->kind != BUFF_KIND_FILE) {
        return;
    }

    dirty = 1;
    paren_hl_find_parens(event->frame);
}

void paren_hl_line_handler(yed_event *event) {
    yed_frame *frame;

    frame = event->frame;

    if (!frame
    ||  frame != ys->active_frame
    ||  !frame->buffer
    ||  frame->buffer->kind != BUFF_KIND_FILE) {
        return;
    }

    if (dirty) {
        paren_hl_find_parens(event->frame);
        dirty = 0;
    }

    paren_hl_hl_parens(event);
}

void paren_hl_find_parens(yed_frame *frame) {
    int        max;
    int        row, col;
    int        first_vis_row, last_vis_row;
    int        balance;
    yed_line  *line;
    yed_glyph *g;

    beg_row = beg_col = end_row = end_col = 0;

    max = LINE_MAX;
    yed_get_var_as_int("paren-hl-max-line-length", &max);

    row           = frame->cursor_line;
    col           = frame->cursor_col;
    first_vis_row = frame->buffer_y_offset + 1;
    last_vis_row  = MIN(frame->buffer_y_offset + frame->height,
                        bucket_array_len(frame->buffer->lines));
    balance       = 0;

    /* Scan backwards. */
    for (; row >= first_vis_row; row -= 1) {
        line = yed_buff_get_line(frame->buffer, row);

        if (line->visual_width == 0) { continue; }

        if (line->visual_width > max) {
            beg_row = beg_col = end_row = end_col = 0;
            return;
        }

        if (row == frame->cursor_line) {
            if (col == 1) { continue; }
            col -= 1;
        } else {
            col = line->visual_width;
        }

        while (col > 0) {
            g = yed_line_col_to_glyph(line, col);

            if (g->c == '(') {
                if (balance == 0) {
                    beg_row = row;
                    beg_col = col;
                    goto done_back;
                } else {
                    balance += 1;
                }
            } else if (g->c == ')') {
                balance -= 1;
            }

            col -= yed_get_glyph_len(*g);
        }
    }
done_back:

    row     = frame->cursor_line;
    col     = frame->cursor_col;
    balance = 0;

    /* Scan forwards. */
    for (; row <= last_vis_row; row += 1) {
        line = yed_buff_get_line(frame->buffer, row);

        if (line->visual_width == 0) { continue; }

        if (line->visual_width > max) {
            beg_row = beg_col = end_row = end_col = 0;
            return;
        }

        if (row != frame->cursor_line) {
            col = 1;
        }

        while (col <= line->visual_width) {
            g = yed_line_col_to_glyph(line, col);

            if (g->c == '(') {
                balance += 1;
            } else if (g->c == ')') {
                if (balance == 0) {
                    end_row = row;
                    end_col = col;
                    goto done_forward;
                } else {
                    balance -= 1;
                }
            }

            col += yed_get_glyph_len(*g);
        }
    }

done_forward:
    return;
}

void paren_hl_hl_parens(yed_event *event) {
    yed_attrs *attr;
    yed_attrs  atn;

    if (beg_row && beg_col && beg_row == event->row) {
        atn = yed_active_style_get_associate();
        yed_eline_combine_col_attrs(event, beg_col, &atn);
    }

    if (end_row && end_col && end_row == event->row) {
        atn = yed_active_style_get_associate();
        yed_eline_combine_col_attrs(event, end_col, &atn);
    }
}
