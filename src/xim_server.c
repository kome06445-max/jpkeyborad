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

static const uint32_t styles_array[] = {
    XCB_IM_PreeditPosition  | XCB_IM_StatusArea,
    XCB_IM_PreeditCallbacks | XCB_IM_StatusCallbacks,
    XCB_IM_PreeditNothing   | XCB_IM_StatusNothing,
    XCB_IM_PreeditNone      | XCB_IM_StatusNone,
};

static const xcb_im_styles_t input_styles = {
    .nStyles = 4,
    .styles = (uint32_t *)styles_array
};

static xcb_im_encoding_t encoding_names[] = { "COMPOUND_TEXT" };

static const xcb_im_encodings_t encodings = {
    .nEncodings = 1,
    .encodings = encoding_names
};

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

static void free_ic_data(xcb_im_input_context_t *ic) {
    ic_data_t *d = get_ic_data(ic);
    if (d) {
        engine_context_destroy(d->engine);
        free(d);
        xcb_im_input_context_set_data(ic, NULL, NULL);
    }
}

static void commit_string(xcb_im_t *im, xcb_im_input_context_t *ic,
                          const char *utf8) {
    if (!utf8 || !*utf8) return;

    size_t compound_len = 0;
    char *compound = xcb_utf8_to_compound_text(utf8, strlen(utf8), &compound_len);
    if (compound) {
        xcb_im_commit_string(im, ic, XCB_XIM_LOOKUP_CHARS,
                             compound, (uint32_t)compound_len, 0);
        free(compound);
    }
}

static void update_preedit(xcb_im_t *im, xcb_im_input_context_t *ic,
                           engine_context_t *eng) {
    const char *preedit = engine_get_preedit(eng);
    if (!preedit || !*preedit) {
        xcb_im_preedit_draw_fr_t frame;
        memset(&frame, 0, sizeof(frame));
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

    xcb_im_preedit_draw_fr_t frame;
    memset(&frame, 0, sizeof(frame));
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

/*
 * Single XIM callback handler.
 * xcb-imdkit dispatches all XIM protocol messages through one callback.
 * We dispatch based on the major opcode in the packet header.
 */
static void xim_callback(xcb_im_t *im, xcb_im_client_t *client,
                          xcb_im_input_context_t *ic,
                          const xcb_im_packet_header_fr_t *hdr,
                          void *frame, void *arg, void *user_data) {
    (void)client;
    (void)arg;
    bsdjp_server_t *srv = (bsdjp_server_t *)user_data;

    uint8_t major = hdr->major_opcode;

    switch (major) {
    case XCB_XIM_CREATE_IC:
        ensure_ic_data(ic);
        bsdjp_log("IC created");
        break;

    case XCB_XIM_DESTROY_IC:
        free_ic_data(ic);
        bsdjp_log("IC destroyed");
        break;

    case XCB_XIM_SET_IC_FOCUS:
        ensure_ic_data(ic);
        xcb_im_preedit_start_callback(im, ic);
        break;

    case XCB_XIM_UNSET_IC_FOCUS:
        break;

    case XCB_XIM_FORWARD_EVENT: {
        ic_data_t *d = ensure_ic_data(ic);
        xcb_key_press_event_t *event = (xcb_key_press_event_t *)frame;

        engine_result_t result = engine_process_key(d->engine, event, srv->conn);

        switch (result.action) {
        case ENGINE_ACTION_COMMIT:
            commit_string(im, ic, result.commit_text);
            update_preedit(im, ic, d->engine);
            candidate_window_hide();
            break;

        case ENGINE_ACTION_PREEDIT:
            update_preedit(im, ic, d->engine);
            if (result.show_candidates && result.candidates &&
                result.num_candidates > 0) {
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
        break;
    }

    case XCB_XIM_RESET_IC: {
        ic_data_t *d = get_ic_data(ic);
        if (d) {
            engine_context_destroy(d->engine);
            d->engine = engine_context_create();
        }
        break;
    }

    default:
        break;
    }
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

    xcb_compound_text_init();

    srv->im = xcb_im_create(
        srv->conn,
        srv->screen_num,
        srv->server_win,
        "BSDJP",
        XCB_IM_ALL_LOCALES,
        &input_styles,
        NULL,          /* on-keys (NULL = forward all) */
        NULL,          /* off-keys */
        &encodings,
        0,             /* event_mask: 0 = XCB_EVENT_MASK_KEY_PRESS */
        xim_callback,
        srv
    );

    if (!srv->im) {
        bsdjp_error("Failed to create XIM server");
        return -1;
    }

    xcb_im_set_use_sync_mode(srv->im, false);

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
