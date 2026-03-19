#include "kana_kanji.h"
#include "dict.h"
#include "user_dict.h"
#include "predict.h"
#include "util.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

static bool candidate_exists(char **candidates, int count, const char *s) {
    for (int i = 0; i < count; i++) {
        if (strcmp(candidates[i], s) == 0)
            return true;
    }
    return false;
}

int kana_kanji_convert(const char *reading, char **candidates, int max) {
    if (!reading || !*reading || max <= 0) return 0;

    int count = 0;

    /* 1. User dictionary (learned entries, sorted by frequency) */
    char *user_cands[KANA_KANJI_MAX_CANDIDATES];
    int user_count = user_dict_lookup(reading, user_cands, max / 2);
    for (int i = 0; i < user_count && count < max; i++) {
        if (!candidate_exists(candidates, count, user_cands[i])) {
            candidates[count++] = user_cands[i];
        } else {
            free(user_cands[i]);
        }
    }

    /* 2. Main dictionary exact match */
    char *dict_cands[KANA_KANJI_MAX_CANDIDATES];
    int dict_count = dict_lookup(&g_dict, reading, dict_cands,
                                 KANA_KANJI_MAX_CANDIDATES);
    for (int i = 0; i < dict_count && count < max; i++) {
        if (!candidate_exists(candidates, count, dict_cands[i])) {
            candidates[count++] = dict_cands[i];
        } else {
            free(dict_cands[i]);
        }
    }

    /* 3. Try prefix decomposition for compound words.
     *    Progressively shorten the reading and look up the prefix. */
    if (count == 0) {
        size_t rlen = utf8_strlen(reading);
        if (rlen > 1) {
            const char *p = reading;
            uint32_t cp;
            for (size_t cut = rlen - 1; cut >= 1 && count < max; cut--) {
                /* Find byte offset for 'cut' characters */
                p = reading;
                for (size_t j = 0; j < cut; j++)
                    p += utf8_decode(p, &cp);
                size_t byte_offset = (size_t)(p - reading);

                char *prefix = bsdjp_strndup(reading, byte_offset);
                char *sub_cands[16];
                int sub_count = dict_lookup(&g_dict, prefix, sub_cands, 16);

                if (sub_count > 0) {
                    /* Found prefix match; append the suffix */
                    const char *suffix = reading + byte_offset;
                    for (int i = 0; i < sub_count && count < max; i++) {
                        size_t total = strlen(sub_cands[i]) + strlen(suffix) + 1;
                        char *compound = bsdjp_malloc(total);
                        snprintf(compound, total, "%s%s", sub_cands[i], suffix);
                        if (!candidate_exists(candidates, count, compound)) {
                            candidates[count++] = compound;
                        } else {
                            free(compound);
                        }
                        free(sub_cands[i]);
                    }
                }
                free(prefix);
                if (count > 0) break;
            }
        }
    }

    /* 4. Prediction candidates */
    char *pred_cands[8];
    int pred_count = predict_candidates(reading, pred_cands, 8);
    for (int i = 0; i < pred_count && count < max; i++) {
        if (!candidate_exists(candidates, count, pred_cands[i])) {
            candidates[count++] = pred_cands[i];
        } else {
            free(pred_cands[i]);
        }
    }

    /* 5. Add original hiragana as fallback */
    if (count < max && !candidate_exists(candidates, count, reading)) {
        candidates[count++] = bsdjp_strdup(reading);
    }

    /* 6. Add katakana version as fallback */
    if (count < max) {
        char kata_buf[1024];
        kana_to_katakana(reading, kata_buf, sizeof(kata_buf));
        if (!candidate_exists(candidates, count, kata_buf)) {
            candidates[count++] = bsdjp_strdup(kata_buf);
        }
    }

    return count;
}
