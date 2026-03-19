#include "xim_server.h"
#include "engine.h"
#include "candidate_window.h"
#include "util.h"

#include <xcb/xcb.h>
#include <xcb/xcb_aux.h>
#include <xcb-imdkit/imdkit.h>
#include <xcb-imdkit/ximproto.h>
#include <xcb-imdkit/encoding.h>
#include <stdlib.h>
#include <string.h>

/* XIM input style we support */
static const xcb_im_styles_t input_styles = {
    .nStyles = 3,
    .styles = (uint32_t[]){
        XCB_IM_PreeditPosition  | XCB_IM_StatusArea,
        XCB_IM_PreeditNothing   | XCB_IM_StatusNothing,
        XCB_IM_PreeditNone      | XCB_IM_StatusNone,
    }
};

static const xcb_im_encodings_t encodings = {
    .nEncodings = 1,
    .encodings = (xcb_im_encoding_t[]){
        { .name = "COMPOUND_TEXT" }
    }
};

/* Per-IC (input context) state, stored via xcb_im_input_context_set_data */
typedef struct {
    engine_context_t *engine;
} ic_data_t;

static ic_data_t *get_ic_data(xcb_im_input_context_t *ic) {
    return (ic_data_t *)xcb_im_input_context_get_data(ic);
}

static ic_data_t *ensure_ic_data(xcb_im_input_context_t *ic) {
    ic_data_t *d = get_ic_data(ic);
    if (!d) {
        d = bsdjp_calloc(1, sizeof(ic_data_t));
        d->engine = engine_context_create();
        xcb_im_input_context_set_data(ic, d, NULL);
    }
    return d;
}

static void commit_string(xcb_im_t *im, xcb_im_input_context_t *ic,
                          const char *utf8) {
    if (!utf8 || !*utf8) return;

    size_t compound_len = 0;
    char *compound = xcb_utf8_to_compound_text(utf8, strlen(utf8), &compound_len);
    if (compound) {
        xcb_im_commit_string(im, ic, XCB_XIM_LOOKUP_CHARS,
                             (char *)compound, compound_len, 0);
        free(compound);
    }
}

static void update_preedit(xcb_im_t *im, xcb_im_input_context_t *ic,
                           engine_context_t *eng) {
    const char *preedit = engine_get_preedit(eng);
    if (!preedit || !*preedit) {
        xcb_im_preedit_draw_fr_t frame = {0};
        frame.caret = 0;
        frame.chg_first = 0;
        frame.chg_length = 0;
        frame.preedit_string = NULL;
        frame.length_of_preedit_string = 0;
        frame.feedback_array.size = 0;
        frame.feedback_array.items = NULL;
        xcb_im_preedit_draw_callback(im, ic, &frame);
        return;
    }

    size_t utf8_len = strlen(preedit);
    size_t compound_len = 0;
    char *compound = xcb_utf8_to_compound_text(preedit, utf8_len, &compound_len);
    if (!compound) return;

    uint32_t text_len = (uint32_t)utf8_strlen(preedit);
    xcb_im_feedback_t *feedback = bsdjp_calloc(text_len, sizeof(xcb_im_feedback_t));
    for (uint32_t i = 0; i < text_len; i++)
        feedback[i] = XCB_XIM_UNDERLINE;

    xcb_im_preedit_draw_fr_t frame = {0};
    frame.caret = (int32_t)text_len;
    frame.chg_first = 0;
    frame.chg_length = 0;
    frame.preedit_string = (uint8_t *)compound;
    frame.length_of_preedit_string = (uint32_t)compound_len;
    frame.feedback_array.size = text_len;
    frame.feedback_array.items = feedback;

    xcb_im_preedit_draw_callback(im, ic, &frame);
    free(compound);
    free(feedback);
}

/* --- XIM Callbacks --- */

static void handle_create_ic(xcb_im_t *im, xcb_im_input_context_t *ic,
                             void *user_data) {
    (void)im; (void)user_data;
    ensure_ic_data(ic);
    bsdjp_log("IC created");
}

static void handle_destroy_ic(xcb_im_t *im, xcb_im_input_context_t *ic,
                              void *user_data) {
    (void)im; (void)user_data;
    ic_data_t *d = get_ic_data(ic);
    if (d) {
        engine_context_destroy(d->engine);
        free(d);
        xcb_im_input_context_set_data(ic, NULL, NULL);
    }
    bsdjp_log("IC destroyed");
}

static void handle_set_ic_focus(xcb_im_t *im, xcb_im_input_context_t *ic,
                                void *user_data) {
    (void)user_data;
    ensure_ic_data(ic);
    xcb_im_preedit_start_callback(im, ic);
}

static void handle_unset_ic_focus(xcb_im_t *im, xcb_im_input_context_t *ic,
                                  void *user_data) {
    (void)user_data;
    (void)ic;
    (void)im;
}

static void handle_forward_event(xcb_im_t *im, xcb_im_input_context_t *ic,
                                 xcb_key_press_event_t *event,
                                 void *user_data) {
    (void)user_data;
    ic_data_t *d = ensure_ic_data(ic);
    bsdjp_server_t *srv = (bsdjp_server_t *)user_data;

    engine_result_t result = engine_process_key(d->engine, event, srv->conn);

    switch (result.action) {
    case ENGINE_ACTION_COMMIT:
        commit_string(im, ic, result.commit_text);
        update_preedit(im, ic, d->engine);
        candidate_window_hide();
        break;

    case ENGINE_ACTION_PREEDIT:
        update_preedit(im, ic, d->engine);
        if (result.show_candidates && result.candidates && result.num_candidates > 0) {
            candidate_window_show(srv->conn, srv->screen,
                                  result.candidates, result.num_candidates,
                                  result.selected_index);
        } else {
            candidate_window_hide();
        }
        break;

    case ENGINE_ACTION_FORWARD:
        xcb_im_forward_event(im, ic, event);
        break;

    case ENGINE_ACTION_CONSUME:
        break;
    }

    engine_result_free(&result);
}

static bool handle_xim_callback(xcb_im_t *im, xcb_im_client_t *client,
                                xcb_im_input_context_t *ic,
                                const xcb_im_packet_header_fr_t *hdr,
                                void *frame, void *arg, void *user_data) {
    (void)im; (void)client; (void)ic;
    (void)hdr; (void)frame; (void)arg; (void)user_data;
    return true;
}

/* --- Public API --- */

bsdjp_server_t *xim_server_create(void) {
    return bsdjp_calloc(1, sizeof(bsdjp_server_t));
}

int xim_server_init(bsdjp_server_t *srv) {
    srv->conn = xcb_connect(NULL, &srv->screen_num);
    if (xcb_connection_has_error(srv->conn)) {
        bsdjp_error("Cannot connect to X server");
        return -1;
    }

    srv->screen = xcb_aux_get_screen(srv->conn, srv->screen_num);
    if (!srv->screen) {
        bsdjp_error("Cannot get screen");
        return -1;
    }

    srv->server_win = xcb_generate_id(srv->conn);
    xcb_create_window(srv->conn, XCB_COPY_FROM_PARENT,
                      srv->server_win, srv->screen->root,
                      0, 0, 1, 1, 0,
                      XCB_WINDOW_CLASS_INPUT_OUTPUT,
                      srv->screen->root_visual, 0, NULL);

    xcb_im_trigger_key_t trigger_on = {
        .keysym = XCB_NO_SYMBOL,
        .modifier = 0,
        .modifier_mask = 0
    };

    srv->im = xcb_im_create(
        srv->conn,
        srv->screen_num,
        srv->server_win,
        "BSDJP",                   /* server name */
        XCB_IM_ALL_LOCALES,
        &input_styles,
        &trigger_on, 1,           /* on-keys */
        NULL, 0,                  /* off-keys */
        &encodings,
        0,                        /* event mask */
        handle_xim_callback,
        srv
    );

    if (!srv->im) {
        bsdjp_error("Failed to create XIM server");
        return -1;
    }

    xcb_im_set_use_sync_mode(srv->im, false);

    /* Register per-IC callbacks */
    xcb_im_set_create_ic_callback(srv->im, handle_create_ic, srv);
    xcb_im_set_destroy_ic_callback(srv->im, handle_destroy_ic, srv);
    xcb_im_set_set_ic_focus_callback(srv->im, handle_set_ic_focus, srv);
    xcb_im_set_unset_ic_focus_callback(srv->im, handle_unset_ic_focus, srv);
    xcb_im_set_forward_event_callback(srv->im, handle_forward_event, srv);

    if (!xcb_im_open_im(srv->im)) {
        bsdjp_error("Failed to open XIM");
        return -1;
    }

    xcb_flush(srv->conn);
    srv->running = true;
    bsdjp_log("XIM server initialized (BSDJP)");
    return 0;
}

void xim_server_run(bsdjp_server_t *srv) {
    bsdjp_log("Entering event loop");

    while (srv->running) {
        xcb_generic_event_t *event = xcb_wait_for_event(srv->conn);
        if (!event) {
            if (xcb_connection_has_error(srv->conn)) {
                bsdjp_error("X connection lost");
                srv->running = false;
            }
            continue;
        }

        if (xcb_im_filter_event(srv->im, event)) {
            free(event);
            continue;
        }

        /* Handle candidate window expose events */
        uint8_t response_type = event->response_type & ~0x80;
        if (response_type == XCB_EXPOSE) {
            candidate_window_handle_expose(srv->conn);
        }

        free(event);
    }
}

void xim_server_stop(bsdjp_server_t *srv) {
    srv->running = false;
}

void xim_server_destroy(bsdjp_server_t *srv) {
    if (!srv) return;
    if (srv->im) {
        xcb_im_close_im(srv->im);
        xcb_im_destroy(srv->im);
    }
    if (srv->conn) {
        if (srv->server_win)
            xcb_destroy_window(srv->conn, srv->server_win);
        xcb_disconnect(srv->conn);
    }
    candidate_window_cleanup();
    free(srv);
    bsdjp_log("Server destroyed");
}
