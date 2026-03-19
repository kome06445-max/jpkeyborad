#ifndef BSDJP_ENGINE_H
#define BSDJP_ENGINE_H

#include <xcb/xcb.h>
#include <stdbool.h>
#include <stdint.h>

/* Forward declarations */
typedef struct engine_context engine_context_t;

typedef enum {
    ENGINE_ACTION_COMMIT,       /* Commit text to client */
    ENGINE_ACTION_PREEDIT,      /* Update preedit display */
    ENGINE_ACTION_FORWARD,      /* Forward key to client */
    ENGINE_ACTION_CONSUME       /* Consume key, no output */
} engine_action_t;

typedef enum {
    INPUT_MODE_HIRAGANA,
    INPUT_MODE_KATAKANA,
    INPUT_MODE_ASCII,
    INPUT_MODE_WIDE_ASCII
} input_mode_t;

typedef enum {
    CONV_STATE_INPUT,           /* Typing romaji / kana */
    CONV_STATE_CONVERTING,      /* Showing conversion candidates */
    CONV_STATE_SELECTING        /* Selecting from candidate list */
} conversion_state_t;

typedef struct {
    engine_action_t action;
    char           *commit_text;
    bool            show_candidates;
    char          **candidates;
    int             num_candidates;
    int             selected_index;
} engine_result_t;

/* Global init/cleanup (loads dictionaries, romaji table) */
int  engine_global_init(void);
void engine_global_cleanup(void);

/* Per-IC context */
engine_context_t *engine_context_create(void);
void              engine_context_destroy(engine_context_t *ctx);

/* Key processing */
engine_result_t engine_process_key(engine_context_t *ctx,
                                   xcb_key_press_event_t *event,
                                   xcb_connection_t *conn);
void engine_result_free(engine_result_t *r);

/* State queries */
const char        *engine_get_preedit(engine_context_t *ctx);
input_mode_t       engine_get_mode(engine_context_t *ctx);
conversion_state_t engine_get_conv_state(engine_context_t *ctx);

#endif /* BSDJP_ENGINE_H */
