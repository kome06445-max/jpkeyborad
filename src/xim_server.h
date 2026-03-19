#ifndef BSDJP_XIM_SERVER_H
#define BSDJP_XIM_SERVER_H

#include <xcb/xcb.h>
#include <xcb-imdkit/imdkit.h>
#include <xcb-imdkit/ximproto.h>
#include <stdbool.h>

typedef struct bsdjp_server bsdjp_server_t;

struct bsdjp_server {
    xcb_connection_t *conn;
    xcb_screen_t     *screen;
    xcb_window_t      server_win;
    xcb_im_t         *im;
    bool              running;
    int               screen_num;
};

/* Lifecycle */
bsdjp_server_t *xim_server_create(void);
int              xim_server_init(bsdjp_server_t *srv);
void             xim_server_run(bsdjp_server_t *srv);
void             xim_server_stop(bsdjp_server_t *srv);
void             xim_server_destroy(bsdjp_server_t *srv);

#endif /* BSDJP_XIM_SERVER_H */
