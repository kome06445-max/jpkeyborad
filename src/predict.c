#include "predict.h"
#include "dict.h"
#include "user_dict.h"
#include "util.h"

#include <string.h>
#include <stdlib.h>

#define PREDICT_HISTORY_SIZE 256

typedef struct {
    char *reading;
    char *result;
    int   frequency;
} predict_entry_t;

static predict_entry_t history[PREDICT_HISTORY_SIZE];
static int history_count = 0;

void predict_init(void) {
    memset(history, 0, sizeof(history));
    history_count = 0;
}

void predict_cleanup(void) {
    for (int i = 0; i < history_count; i++) {
        free(history[i].reading);
        free(history[i].result);
    }
    history_count = 0;
}

/* Record a conversion for future predictions.
 * Called externally when user confirms a conversion. */
void predict_record(const char *reading, const char *result) {
    if (!reading || !result) return;

    for (int i = 0; i < history_count; i++) {
        if (strcmp(history[i].reading, reading) == 0 &&
            strcmp(history[i].result, result) == 0) {
            history[i].frequency++;
            return;
        }
    }

    if (history_count >= PREDICT_HISTORY_SIZE) {
        /* Evict oldest (first) entry */
        free(history[0].reading);
        free(history[0].result);
        memmove(history, history + 1,
                (size_t)(PREDICT_HISTORY_SIZE - 1) * sizeof(predict_entry_t));
        history_count--;
    }

    history[history_count].reading = bsdjp_strdup(reading);
    history[history_count].result = bsdjp_strdup(result);
    history[history_count].frequency = 1;
    history_count++;
}

static bool already_in(char **cands, int count, const char *s) {
    for (int i = 0; i < count; i++) {
        if (strcmp(cands[i], s) == 0) return true;
    }
    return false;
}

int predict_candidates(const char *partial_reading,
                       char **candidates, int max) {
    if (!partial_reading || !*partial_reading || max <= 0) return 0;

    size_t plen = strlen(partial_reading);
    int count = 0;

    /* 1. Check prediction history for prefix matches */
    for (int i = history_count - 1; i >= 0 && count < max; i--) {
        if (strncmp(history[i].reading, partial_reading, plen) == 0 &&
            strlen(history[i].reading) > plen) {
            if (!already_in(candidates, count, history[i].result)) {
                candidates[count++] = bsdjp_strdup(history[i].result);
            }
        }
    }

    /* 2. Check user dictionary for prefix matches */
    char *user_cands[16];
    int user_count = user_dict_lookup(partial_reading, user_cands, 16);
    for (int i = 0; i < user_count && count < max; i++) {
        if (!already_in(candidates, count, user_cands[i])) {
            candidates[count++] = user_cands[i];
        } else {
            free(user_cands[i]);
        }
    }

    return count;
}
