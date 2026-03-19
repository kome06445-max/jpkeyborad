#include "candidate_window.h"
#include "util.h"

#include <xcb/xcb.h>
#include <cairo/cairo.h>
#include <cairo/cairo-xcb.h>
#include <pango/pangocairo.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

/* Stringify macro helper */
#define BSDJP_STR_INNER(x) #x
#define BSDJP_STR(x) BSDJP_STR_INNER(x)

#define CW_FONT_NAME  "IPAGothic"
#define CW_FONT_SIZE  14
#define CW_PADDING    8
#define CW_ITEM_HEIGHT 28
#define CW_MIN_WIDTH  200
#define CW_MAX_DISPLAY 9
#define CW_BORDER     1

/* Colors */
#define CW_BG_R    1.0
#define CW_BG_G    1.0
#define CW_BG_B    1.0

#define CW_FG_R    0.1
#define CW_FG_G    0.1
#define CW_FG_B    0.1

#define CW_SEL_BG_R 0.2
#define CW_SEL_BG_G 0.4
#define CW_SEL_BG_B 0.8

#define CW_SEL_FG_R 1.0
#define CW_SEL_FG_G 1.0
#define CW_SEL_FG_B 1.0

#define CW_BORDER_R 0.6
#define CW_BORDER_G 0.6
#define CW_BORDER_B 0.6

#define CW_IDX_FG_R 0.5
#define CW_IDX_FG_G 0.5
#define CW_IDX_FG_B 0.5

static struct {
    xcb_connection_t *conn;
    xcb_screen_t     *screen;
    xcb_window_t      win;
    xcb_visualtype_t *visual;
    bool              created;
    bool              visible;

    char **candidates;
    int    count;
    int    selected;
    int    scroll_offset;

    int    width;
    int    height;
} cw = {0};

static xcb_visualtype_t *find_visual(xcb_screen_t *screen) {
    xcb_depth_iterator_t depth_iter = xcb_screen_allowed_depths_iterator(screen);
    for (; depth_iter.rem; xcb_depth_next(&depth_iter)) {
        xcb_visualtype_iterator_t vis_iter =
            xcb_depth_visuals_iterator(depth_iter.data);
        for (; vis_iter.rem; xcb_visualtype_next(&vis_iter)) {
            if (screen->root_visual == vis_iter.data->visual_id)
                return vis_iter.data;
        }
    }
    return NULL;
}

static void create_window(void) {
    if (cw.created) return;

    cw.visual = find_visual(cw.screen);
    if (!cw.visual) {
        bsdjp_error("Cannot find root visual for candidate window");
        return;
    }

    cw.win = xcb_generate_id(cw.conn);

    uint32_t mask = XCB_CW_BACK_PIXEL |
                    XCB_CW_OVERRIDE_REDIRECT |
                    XCB_CW_EVENT_MASK;
    uint32_t values[] = {
        cw.screen->white_pixel,
        1,  /* override redirect: popup behavior */
        XCB_EVENT_MASK_EXPOSURE
    };

    xcb_create_window(cw.conn, XCB_COPY_FROM_PARENT,
                      cw.win, cw.screen->root,
                      0, 0, CW_MIN_WIDTH, CW_ITEM_HEIGHT,
                      0, XCB_WINDOW_CLASS_INPUT_OUTPUT,
                      cw.screen->root_visual, mask, values);

    cw.created = true;
}

static void draw_candidates(void) {
    if (!cw.created || !cw.visible) return;

    int display_count = cw.count;
    if (display_count > CW_MAX_DISPLAY)
        display_count = CW_MAX_DISPLAY;

    cw.width = CW_MIN_WIDTH;
    cw.height = display_count * CW_ITEM_HEIGHT + CW_PADDING * 2;

    /* Create Cairo surface for drawing */
    cairo_surface_t *surface = cairo_xcb_surface_create(
        cw.conn, cw.win, cw.visual,
        cw.width, cw.height);
    cairo_t *cr = cairo_create(surface);

    /* Background */
    cairo_set_source_rgb(cr, CW_BG_R, CW_BG_G, CW_BG_B);
    cairo_paint(cr);

    /* Border */
    cairo_set_source_rgb(cr, CW_BORDER_R, CW_BORDER_G, CW_BORDER_B);
    cairo_set_line_width(cr, CW_BORDER);
    cairo_rectangle(cr, 0.5, 0.5, cw.width - 1, cw.height - 1);
    cairo_stroke(cr);

    /* Create Pango layout */
    PangoLayout *layout = pango_cairo_create_layout(cr);
    PangoFontDescription *font = pango_font_description_from_string(
        CW_FONT_NAME " " BSDJP_STR(CW_FONT_SIZE));

    /* Calculate scroll offset */
    if (cw.selected < cw.scroll_offset)
        cw.scroll_offset = cw.selected;
    if (cw.selected >= cw.scroll_offset + CW_MAX_DISPLAY)
        cw.scroll_offset = cw.selected - CW_MAX_DISPLAY + 1;

    /* Draw each candidate */
    for (int i = 0; i < display_count; i++) {
        int actual_idx = i + cw.scroll_offset;
        if (actual_idx >= cw.count) break;

        int y = CW_PADDING + i * CW_ITEM_HEIGHT;
        bool is_selected = (actual_idx == cw.selected);

        /* Selection highlight */
        if (is_selected) {
            cairo_set_source_rgb(cr, CW_SEL_BG_R, CW_SEL_BG_G, CW_SEL_BG_B);
            cairo_rectangle(cr, CW_BORDER, y,
                          cw.width - CW_BORDER * 2, CW_ITEM_HEIGHT);
            cairo_fill(cr);
        }

        /* Index number (1-9) */
        char idx_str[8];
        snprintf(idx_str, sizeof(idx_str), "%d.", (i + 1));

        if (is_selected) {
            cairo_set_source_rgb(cr, CW_SEL_FG_R, CW_SEL_FG_G, CW_SEL_FG_B);
        } else {
            cairo_set_source_rgb(cr, CW_IDX_FG_R, CW_IDX_FG_G, CW_IDX_FG_B);
        }

        pango_layout_set_font_description(layout, font);
        pango_layout_set_text(layout, idx_str, -1);
        cairo_move_to(cr, CW_PADDING, y + (CW_ITEM_HEIGHT - CW_FONT_SIZE) / 2.0);
        pango_cairo_show_layout(cr, layout);

        /* Candidate text */
        if (is_selected) {
            cairo_set_source_rgb(cr, CW_SEL_FG_R, CW_SEL_FG_G, CW_SEL_FG_B);
        } else {
            cairo_set_source_rgb(cr, CW_FG_R, CW_FG_G, CW_FG_B);
        }

        pango_layout_set_text(layout, cw.candidates[actual_idx], -1);
        cairo_move_to(cr, CW_PADDING + 30,
                     y + (CW_ITEM_HEIGHT - CW_FONT_SIZE) / 2.0);
        pango_cairo_show_layout(cr, layout);

        /* Measure text width to auto-size window */
        int text_w, text_h;
        pango_layout_get_pixel_size(layout, &text_w, &text_h);
        int needed_w = text_w + CW_PADDING * 2 + 40;
        if (needed_w > cw.width)
            cw.width = needed_w;
    }

    /* Scroll indicator */
    if (cw.count > CW_MAX_DISPLAY) {
        char indicator[32];
        snprintf(indicator, sizeof(indicator), "[%d/%d]",
                cw.selected + 1, cw.count);
        cairo_set_source_rgb(cr, CW_IDX_FG_R, CW_IDX_FG_G, CW_IDX_FG_B);
        PangoFontDescription *small_font = pango_font_description_from_string(
            CW_FONT_NAME " 10");
        pango_layout_set_font_description(layout, small_font);
        pango_layout_set_text(layout, indicator, -1);
        cairo_move_to(cr, cw.width - 60, cw.height - 18);
        pango_cairo_show_layout(cr, layout);
        pango_font_description_free(small_font);
    }

    g_object_unref(layout);
    pango_font_description_free(font);
    cairo_destroy(cr);
    cairo_surface_destroy(surface);

    xcb_flush(cw.conn);
}

void candidate_window_show(xcb_connection_t *conn,
                           xcb_screen_t *screen,
                           char **candidates, int count,
                           int selected_index) {
    cw.conn = conn;
    cw.screen = screen;

    /* Copy candidates */
    if (cw.candidates) {
        for (int i = 0; i < cw.count; i++)
            free(cw.candidates[i]);
        free(cw.candidates);
    }

    cw.count = count;
    cw.selected = selected_index;
    cw.scroll_offset = 0;
    cw.candidates = bsdjp_malloc(sizeof(char *) * (size_t)count);
    for (int i = 0; i < count; i++)
        cw.candidates[i] = bsdjp_strdup(candidates[i]);

    create_window();

    int display_count = count;
    if (display_count > CW_MAX_DISPLAY)
        display_count = CW_MAX_DISPLAY;

    cw.width = CW_MIN_WIDTH;
    cw.height = display_count * CW_ITEM_HEIGHT + CW_PADDING * 2;

    /* Query pointer position for window placement */
    xcb_query_pointer_cookie_t pc = xcb_query_pointer(conn, screen->root);
    xcb_query_pointer_reply_t *pr = xcb_query_pointer_reply(conn, pc, NULL);

    int16_t x = 100, y = 100;
    if (pr) {
        x = pr->root_x;
        y = pr->root_y + 20;

        /* Keep window on screen */
        if (x + cw.width > (int)screen->width_in_pixels)
            x = (int16_t)(screen->width_in_pixels - cw.width);
        if (y + cw.height > (int)screen->height_in_pixels)
            y = (int16_t)(pr->root_y - cw.height - 5);
        free(pr);
    }

    /* Resize and reposition */
    uint32_t config_values[] = {
        (uint32_t)x, (uint32_t)y,
        (uint32_t)cw.width, (uint32_t)cw.height
    };
    xcb_configure_window(conn, cw.win,
                        XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y |
                        XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT,
                        config_values);

    xcb_map_window(conn, cw.win);
    cw.visible = true;

    draw_candidates();
    xcb_flush(conn);
}

void candidate_window_hide(void) {
    if (!cw.created || !cw.visible) return;
    xcb_unmap_window(cw.conn, cw.win);
    cw.visible = false;
    xcb_flush(cw.conn);
}

void candidate_window_handle_expose(xcb_connection_t *conn) {
    (void)conn;
    if (cw.visible)
        draw_candidates();
}

void candidate_window_cleanup(void) {
    if (cw.candidates) {
        for (int i = 0; i < cw.count; i++)
            free(cw.candidates[i]);
        free(cw.candidates);
        cw.candidates = NULL;
    }
    if (cw.created && cw.conn) {
        xcb_destroy_window(cw.conn, cw.win);
        cw.created = false;
    }
    cw.visible = false;
}
