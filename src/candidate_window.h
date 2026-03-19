#ifndef BSDJP_CANDIDATE_WINDOW_H
#define BSDJP_CANDIDATE_WINDOW_H

#include <xcb/xcb.h>

/*
 * Candidate window: displays conversion candidates using Cairo + Pango.
 * Drawn as an override-redirect window (popup) on X11.
 */

/* Show the candidate window with the given candidates */
void candidate_window_show(xcb_connection_t *conn,
                           xcb_screen_t *screen,
                           char **candidates, int count,
                           int selected_index);

/* Hide the candidate window */
void candidate_window_hide(void);

/* Handle expose events for redrawing */
void candidate_window_handle_expose(xcb_connection_t *conn);

/* Cleanup resources */
void candidate_window_cleanup(void);

#endif /* BSDJP_CANDIDATE_WINDOW_H */
