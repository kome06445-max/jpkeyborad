#include "engine.h"
#include "romaji.h"
#include "kana_kanji.h"
#include "predict.h"
#include "dict.h"
#include "user_dict.h"
#include "util.h"

#include <xcb/xcb.h>
#include <xcb/xcb_keysyms.h>
#include <X11/keysym.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define PREEDIT_BUF_SIZE   1024
#define ROMAJI_BUF_SIZE    16
#define CANDIDATE_MAX      64

struct engine_context {
    input_mode_t      mode;
    conversion_state_t conv_state;

    /* Romaji input buffer (raw ASCII being typed) */
    char romaji_buf[ROMAJI_BUF_SIZE];
    int  romaji_len;

    /* Kana preedit buffer (converted kana waiting for commit/conversion) */
    char kana_buf[PREEDIT_BUF_SIZE];
    int  kana_len;

    /* Combined preedit string for display (kana + trailing romaji) */
    char preedit[PREEDIT_BUF_SIZE];

    /* Conversion candidates */
    char *candidates[CANDIDATE_MAX];
    int   num_candidates;
    int   selected;
};

/* Rebuild preedit string from kana_buf + romaji_buf */
static void rebuild_preedit(engine_context_t *ctx) {
    snprintf(ctx->preedit, PREEDIT_BUF_SIZE, "%s%s",
             ctx->kana_buf, ctx->romaji_buf);
}

static void clear_romaji(engine_context_t *ctx) {
    memset(ctx->romaji_buf, 0, ROMAJI_BUF_SIZE);
    ctx->romaji_len = 0;
}

static void clear_kana(engine_context_t *ctx) {
    memset(ctx->kana_buf, 0, PREEDIT_BUF_SIZE);
    ctx->kana_len = 0;
}

static void clear_candidates(engine_context_t *ctx) {
    for (int i = 0; i < ctx->num_candidates; i++)
        free(ctx->candidates[i]);
    ctx->num_candidates = 0;
    ctx->selected = 0;
}

static void clear_all(engine_context_t *ctx) {
    clear_romaji(ctx);
    clear_kana(ctx);
    clear_candidates(ctx);
    ctx->conv_state = CONV_STATE_INPUT;
    ctx->preedit[0] = '\0';
}

/* Flush romaji buffer: try converting current romaji input to kana */
static void flush_romaji(engine_context_t *ctx) {
    while (ctx->romaji_len > 0) {
        bool use_kata = (ctx->mode == INPUT_MODE_KATAKANA);
        char output[ROMAJI_MAX_OUTPUT];
        bool pending = false;

        /* Try sokuon: doubled consonant */
        if (ctx->romaji_len >= 2 &&
            romaji_is_sokuon(ctx->romaji_buf[0], ctx->romaji_buf[1])) {
            const char *sokuon = use_kata ? "ッ" : "っ";
            size_t slen = strlen(sokuon);
            if (ctx->kana_len + (int)slen < PREEDIT_BUF_SIZE) {
                memcpy(ctx->kana_buf + ctx->kana_len, sokuon, slen);
                ctx->kana_len += (int)slen;
                ctx->kana_buf[ctx->kana_len] = '\0';
            }
            /* Remove first char, keep the rest */
            memmove(ctx->romaji_buf, ctx->romaji_buf + 1,
                    (size_t)(ctx->romaji_len - 1));
            ctx->romaji_len--;
            ctx->romaji_buf[ctx->romaji_len] = '\0';
            continue;
        }

        int consumed = romaji_convert(&g_romaji_table,
                                      ctx->romaji_buf, output, sizeof(output),
                                      use_kata, &pending);
        if (consumed > 0) {
            size_t olen = strlen(output);
            if (ctx->kana_len + (int)olen < PREEDIT_BUF_SIZE) {
                memcpy(ctx->kana_buf + ctx->kana_len, output, olen);
                ctx->kana_len += (int)olen;
                ctx->kana_buf[ctx->kana_len] = '\0';
            }
            memmove(ctx->romaji_buf, ctx->romaji_buf + consumed,
                    (size_t)(ctx->romaji_len - consumed));
            ctx->romaji_len -= consumed;
            ctx->romaji_buf[ctx->romaji_len] = '\0';
        } else {
            break;
        }
    }
}

/* Handle 'n' before non-vowel: standalone ん */
static void handle_n_before_consonant(engine_context_t *ctx, char next) {
    if (ctx->romaji_len == 1 && ctx->romaji_buf[0] == 'n') {
        bool is_vowel = (next == 'a' || next == 'i' || next == 'u' ||
                         next == 'e' || next == 'o' || next == 'y' ||
                         next == 'n');
        if (!is_vowel && next != '\0') {
            bool use_kata = (ctx->mode == INPUT_MODE_KATAKANA);
            const char *n_kana = use_kata ? "ン" : "ん";
            size_t nlen = strlen(n_kana);
            if (ctx->kana_len + (int)nlen < PREEDIT_BUF_SIZE) {
                memcpy(ctx->kana_buf + ctx->kana_len, n_kana, nlen);
                ctx->kana_len += (int)nlen;
                ctx->kana_buf[ctx->kana_len] = '\0';
            }
            clear_romaji(ctx);
        }
    }
}

static engine_result_t make_result(engine_action_t action) {
    engine_result_t r = {0};
    r.action = action;
    return r;
}

static engine_result_t make_commit(engine_context_t *ctx, const char *text) {
    engine_result_t r = {0};
    r.action = ENGINE_ACTION_COMMIT;
    r.commit_text = bsdjp_strdup(text);
    clear_all(ctx);
    return r;
}

static engine_result_t make_preedit(engine_context_t *ctx) {
    engine_result_t r = {0};
    r.action = ENGINE_ACTION_PREEDIT;

    if (ctx->conv_state == CONV_STATE_SELECTING && ctx->num_candidates > 0) {
        r.show_candidates = true;
        r.num_candidates = ctx->num_candidates;
        r.candidates = bsdjp_malloc(sizeof(char *) * (size_t)ctx->num_candidates);
        for (int i = 0; i < ctx->num_candidates; i++)
            r.candidates[i] = bsdjp_strdup(ctx->candidates[i]);
        r.selected_index = ctx->selected;
    }
    return r;
}

/* --- Key handling for each conversion state --- */

static engine_result_t handle_input_key(engine_context_t *ctx,
                                        xcb_keysym_t keysym,
                                        const char *str, int str_len) {
    /* Enter: commit current preedit */
    if (keysym == XK_Return || keysym == XK_KP_Enter) {
        if (ctx->kana_len > 0 || ctx->romaji_len > 0) {
            flush_romaji(ctx);
            rebuild_preedit(ctx);
            return make_commit(ctx, ctx->preedit);
        }
        return make_result(ENGINE_ACTION_FORWARD);
    }

    /* Escape: cancel */
    if (keysym == XK_Escape) {
        if (ctx->kana_len > 0 || ctx->romaji_len > 0) {
            clear_all(ctx);
            rebuild_preedit(ctx);
            return make_preedit(ctx);
        }
        return make_result(ENGINE_ACTION_FORWARD);
    }

    /* Backspace */
    if (keysym == XK_BackSpace) {
        if (ctx->romaji_len > 0) {
            ctx->romaji_buf[--ctx->romaji_len] = '\0';
            rebuild_preedit(ctx);
            return make_preedit(ctx);
        }
        if (ctx->kana_len > 0) {
            /* Remove last UTF-8 character from kana buffer */
            const char *p = ctx->kana_buf;
            const char *last = p;
            uint32_t cp;
            while (*p) {
                last = p;
                p += utf8_decode(p, &cp);
            }
            ctx->kana_len = (int)(last - ctx->kana_buf);
            ctx->kana_buf[ctx->kana_len] = '\0';
            rebuild_preedit(ctx);
            return make_preedit(ctx);
        }
        return make_result(ENGINE_ACTION_FORWARD);
    }

    /* Space: if we have kana, start conversion */
    if (keysym == XK_space) {
        if (ctx->kana_len > 0 || ctx->romaji_len > 0) {
            flush_romaji(ctx);
            if (ctx->kana_len > 0) {
                clear_candidates(ctx);
                ctx->num_candidates = kana_kanji_convert(
                    ctx->kana_buf, ctx->candidates, CANDIDATE_MAX);
                if (ctx->num_candidates > 0) {
                    ctx->conv_state = CONV_STATE_SELECTING;
                    ctx->selected = 0;
                    /* Show first candidate in preedit */
                    strncpy(ctx->preedit, ctx->candidates[0],
                            PREEDIT_BUF_SIZE - 1);
                    return make_preedit(ctx);
                }
                /* No candidates: commit kana as-is */
                rebuild_preedit(ctx);
                return make_commit(ctx, ctx->preedit);
            }
        }
        return make_result(ENGINE_ACTION_FORWARD);
    }

    /* F6: force hiragana mode */
    if (keysym == XK_F6) {
        ctx->mode = INPUT_MODE_HIRAGANA;
        return make_result(ENGINE_ACTION_CONSUME);
    }

    /* F7: force katakana mode */
    if (keysym == XK_F7) {
        if (ctx->kana_len > 0) {
            /* Convert current kana to katakana and commit */
            char kata[PREEDIT_BUF_SIZE];
            kana_to_katakana(ctx->kana_buf, kata, sizeof(kata));
            return make_commit(ctx, kata);
        }
        ctx->mode = INPUT_MODE_KATAKANA;
        return make_result(ENGINE_ACTION_CONSUME);
    }

    /* F8: half-width katakana (not yet implemented, just switch mode) */
    if (keysym == XK_F8) {
        ctx->mode = INPUT_MODE_KATAKANA;
        return make_result(ENGINE_ACTION_CONSUME);
    }

    /* F10: ASCII mode toggle */
    if (keysym == XK_F10) {
        ctx->mode = (ctx->mode == INPUT_MODE_ASCII)
                    ? INPUT_MODE_HIRAGANA : INPUT_MODE_ASCII;
        return make_result(ENGINE_ACTION_CONSUME);
    }

    /* Zenkaku/Hankaku key toggle */
    if (keysym == XK_Zenkaku_Hankaku || keysym == XK_Henkan_Mode) {
        ctx->mode = (ctx->mode == INPUT_MODE_ASCII)
                    ? INPUT_MODE_HIRAGANA : INPUT_MODE_ASCII;
        return make_result(ENGINE_ACTION_CONSUME);
    }

    /* ASCII mode: forward everything */
    if (ctx->mode == INPUT_MODE_ASCII) {
        return make_result(ENGINE_ACTION_FORWARD);
    }

    /* Printable ASCII character: romaji input */
    if (str_len == 1 && str[0] >= 0x20 && str[0] <= 0x7e) {
        char c = (char)tolower(str[0]);

        handle_n_before_consonant(ctx, c);

        if (ctx->romaji_len < ROMAJI_BUF_SIZE - 1) {
            ctx->romaji_buf[ctx->romaji_len++] = c;
            ctx->romaji_buf[ctx->romaji_len] = '\0';
        }

        /* Try converting */
        bool use_kata = (ctx->mode == INPUT_MODE_KATAKANA);
        char output[ROMAJI_MAX_OUTPUT];
        bool pending = false;

        /* Handle sokuon first */
        if (ctx->romaji_len >= 2 &&
            romaji_is_sokuon(ctx->romaji_buf[0], ctx->romaji_buf[1])) {
            const char *sokuon = use_kata ? "ッ" : "っ";
            size_t slen = strlen(sokuon);
            if (ctx->kana_len + (int)slen < PREEDIT_BUF_SIZE) {
                memcpy(ctx->kana_buf + ctx->kana_len, sokuon, slen);
                ctx->kana_len += (int)slen;
                ctx->kana_buf[ctx->kana_len] = '\0';
            }
            memmove(ctx->romaji_buf, ctx->romaji_buf + 1,
                    (size_t)(ctx->romaji_len - 1));
            ctx->romaji_len--;
            ctx->romaji_buf[ctx->romaji_len] = '\0';
        }

        int consumed = romaji_convert(&g_romaji_table,
                                      ctx->romaji_buf, output, sizeof(output),
                                      use_kata, &pending);
        if (consumed > 0) {
            size_t olen = strlen(output);
            if (ctx->kana_len + (int)olen < PREEDIT_BUF_SIZE) {
                memcpy(ctx->kana_buf + ctx->kana_len, output, olen);
                ctx->kana_len += (int)olen;
                ctx->kana_buf[ctx->kana_len] = '\0';
            }
            memmove(ctx->romaji_buf, ctx->romaji_buf + consumed,
                    (size_t)(ctx->romaji_len - consumed));
            ctx->romaji_len -= consumed;
            ctx->romaji_buf[ctx->romaji_len] = '\0';
        }

        rebuild_preedit(ctx);
        return make_preedit(ctx);
    }

    return make_result(ENGINE_ACTION_FORWARD);
}

static engine_result_t handle_selecting_key(engine_context_t *ctx,
                                            xcb_keysym_t keysym) {
    /* Space / Down: next candidate */
    if (keysym == XK_space || keysym == XK_Down) {
        if (ctx->selected < ctx->num_candidates - 1)
            ctx->selected++;
        else
            ctx->selected = 0;
        strncpy(ctx->preedit, ctx->candidates[ctx->selected],
                PREEDIT_BUF_SIZE - 1);
        return make_preedit(ctx);
    }

    /* Up: previous candidate */
    if (keysym == XK_Up) {
        if (ctx->selected > 0)
            ctx->selected--;
        else
            ctx->selected = ctx->num_candidates - 1;
        strncpy(ctx->preedit, ctx->candidates[ctx->selected],
                PREEDIT_BUF_SIZE - 1);
        return make_preedit(ctx);
    }

    /* Enter: confirm selection */
    if (keysym == XK_Return || keysym == XK_KP_Enter) {
        const char *chosen = ctx->candidates[ctx->selected];
        user_dict_record(ctx->kana_buf, chosen);
        return make_commit(ctx, chosen);
    }

    /* Escape: cancel conversion, go back to kana input */
    if (keysym == XK_Escape) {
        clear_candidates(ctx);
        ctx->conv_state = CONV_STATE_INPUT;
        rebuild_preedit(ctx);
        return make_preedit(ctx);
    }

    /* Number keys 1-9: direct selection */
    if (keysym >= XK_1 && keysym <= XK_9) {
        int idx = (int)(keysym - XK_1);
        if (idx < ctx->num_candidates) {
            ctx->selected = idx;
            const char *chosen = ctx->candidates[ctx->selected];
            user_dict_record(ctx->kana_buf, chosen);
            return make_commit(ctx, chosen);
        }
    }

    /* Backspace: cancel conversion */
    if (keysym == XK_BackSpace) {
        clear_candidates(ctx);
        ctx->conv_state = CONV_STATE_INPUT;
        rebuild_preedit(ctx);
        return make_preedit(ctx);
    }

    return make_result(ENGINE_ACTION_CONSUME);
}

/* --- Public API --- */

int engine_global_init(void) {
    char *table_path = bsdjp_data_path("romaji_table.txt");
    if (romaji_table_load(&g_romaji_table, table_path) != 0) {
        free(table_path);
        return -1;
    }
    free(table_path);

    if (dict_global_init() != 0) {
        bsdjp_error("Dictionary init failed (non-fatal, conversion disabled)");
    }

    user_dict_init();
    predict_init();

    bsdjp_log("Engine initialized");
    return 0;
}

void engine_global_cleanup(void) {
    romaji_table_free(&g_romaji_table);
    dict_global_cleanup();
    user_dict_cleanup();
    predict_cleanup();
}

engine_context_t *engine_context_create(void) {
    engine_context_t *ctx = bsdjp_calloc(1, sizeof(engine_context_t));
    ctx->mode = INPUT_MODE_HIRAGANA;
    ctx->conv_state = CONV_STATE_INPUT;
    return ctx;
}

void engine_context_destroy(engine_context_t *ctx) {
    if (!ctx) return;
    clear_candidates(ctx);
    free(ctx);
}

engine_result_t engine_process_key(engine_context_t *ctx,
                                   xcb_key_press_event_t *event,
                                   xcb_connection_t *conn) {
    xcb_key_symbols_t *syms = xcb_key_symbols_alloc(conn);
    xcb_keysym_t keysym = xcb_key_symbols_get_keysym(syms, event->detail, 0);
    xcb_key_symbols_free(syms);

    /* Get the ASCII string for this key (for romaji input) */
    char str[8] = {0};
    int str_len = 0;

    if (keysym >= XK_space && keysym <= XK_asciitilde) {
        str[0] = (char)keysym;
        str_len = 1;

        /* Handle shift for uppercase letters */
        if (event->state & XCB_MOD_MASK_SHIFT) {
            if (keysym >= XK_a && keysym <= XK_z)
                str[0] = (char)(keysym - XK_a + XK_A);
        }
    }

    /* Ctrl/Alt modifier: forward to app */
    if (event->state & (XCB_MOD_MASK_CONTROL | XCB_MOD_MASK_1)) {
        return make_result(ENGINE_ACTION_FORWARD);
    }

    switch (ctx->conv_state) {
    case CONV_STATE_INPUT:
        return handle_input_key(ctx, keysym, str, str_len);

    case CONV_STATE_CONVERTING:
    case CONV_STATE_SELECTING:
        return handle_selecting_key(ctx, keysym);
    }

    return make_result(ENGINE_ACTION_FORWARD);
}

void engine_result_free(engine_result_t *r) {
    free(r->commit_text);
    r->commit_text = NULL;
    if (r->candidates) {
        for (int i = 0; i < r->num_candidates; i++)
            free(r->candidates[i]);
        free(r->candidates);
        r->candidates = NULL;
    }
}

const char *engine_get_preedit(engine_context_t *ctx) {
    return ctx->preedit;
}

input_mode_t engine_get_mode(engine_context_t *ctx) {
    return ctx->mode;
}

conversion_state_t engine_get_conv_state(engine_context_t *ctx) {
    return ctx->conv_state;
}

/* Hiragana to katakana conversion utility */
void kana_to_katakana(const char *hiragana, char *out, size_t out_size) {
    const char *p = hiragana;
    size_t pos = 0;
    uint32_t cp;

    while (*p && pos + 4 < out_size) {
        int len = utf8_decode(p, &cp);
        /* Hiragana range: U+3041 - U+3096 -> Katakana: U+30A1 - U+30F6 */
        if (cp >= 0x3041 && cp <= 0x3096) {
            cp += 0x60;  /* Shift to katakana */
        }
        int written = utf8_encode(cp, out + pos);
        pos += (size_t)written;
        p += len;
    }
    out[pos] = '\0';
}
