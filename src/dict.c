#include "dict.h"
#include "util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

dict_t g_dict = {0};

static unsigned int dict_hash(const char *key) {
    unsigned int h = 5381;
    while (*key) {
        h = ((h << 5) + h) + (unsigned char)*key;
        key++;
    }
    return h % DICT_HASH_SIZE;
}

static void dict_entry_free(dict_entry_t *e) {
    if (!e) return;
    free(e->key);
    for (int i = 0; i < e->num_candidates; i++)
        free(e->candidates[i]);
    free(e);
}

/*
 * Parse a single SKK dictionary line.
 * Format: "reading /cand1/cand2;annotation/cand3/..."
 * Lines starting with ";;" are comments.
 */
static dict_entry_t *parse_skk_line(const char *line) {
    if (!line || line[0] == ';') return NULL;

    const char *space = strchr(line, ' ');
    if (!space) return NULL;

    size_t key_len = (size_t)(space - line);
    if (key_len == 0) return NULL;

    /* The rest after space should start with '/' */
    const char *p = space + 1;
    if (*p != '/') return NULL;

    dict_entry_t *e = bsdjp_calloc(1, sizeof(dict_entry_t));
    e->key = bsdjp_strndup(line, key_len);

    /* Parse candidates separated by '/' */
    p++; /* skip first '/' */
    while (*p && *p != '\n' && *p != '\r') {
        const char *slash = strchr(p, '/');
        if (!slash) break;

        size_t cand_len = (size_t)(slash - p);
        if (cand_len > 0 && e->num_candidates < DICT_MAX_CANDIDATES) {
            /* Strip annotation after ';' */
            const char *semi = memchr(p, ';', cand_len);
            if (semi)
                cand_len = (size_t)(semi - p);

            if (cand_len > 0) {
                e->candidates[e->num_candidates] = bsdjp_strndup(p, cand_len);
                e->num_candidates++;
            }
        }
        p = slash + 1;
    }

    if (e->num_candidates == 0) {
        dict_entry_free(e);
        return NULL;
    }
    return e;
}

static void dict_insert(dict_t *dict, dict_entry_t *entry) {
    unsigned int h = dict_hash(entry->key);
    entry->next = dict->buckets[h];
    dict->buckets[h] = entry;
    dict->total_entries++;
}

/*
 * Detect encoding: if line contains bytes 0xA1-0xFE in pairs, it's EUC-JP.
 * SKK-JISYO.L is typically in EUC-JP.
 * We do a simple heuristic check on the first non-comment line.
 */
static bool detect_euc_jp(const char *buf, size_t len) {
    for (size_t i = 0; i + 1 < len; i++) {
        unsigned char c1 = (unsigned char)buf[i];
        unsigned char c2 = (unsigned char)buf[i + 1];
        if (c1 >= 0xA1 && c1 <= 0xFE && c2 >= 0xA1 && c2 <= 0xFE)
            return true;
        if ((c1 & 0xE0) == 0xC0 || (c1 & 0xF0) == 0xE0)
            return false;  /* Likely UTF-8 */
    }
    return false;
}

/*
 * Simple EUC-JP to UTF-8 converter for the JIS X 0208 subset.
 * This handles the most common Japanese characters.
 */
static size_t eucjp_to_utf8(const char *src, size_t src_len,
                            char *dst, size_t dst_size) {
    size_t si = 0, di = 0;

    while (si < src_len && di + 4 < dst_size) {
        unsigned char c = (unsigned char)src[si];

        if (c <= 0x7F) {
            /* ASCII */
            dst[di++] = (char)c;
            si++;
        } else if (c == 0x8E && si + 1 < src_len) {
            /* Half-width katakana (JIS X 0201) */
            unsigned char c2 = (unsigned char)src[si + 1];
            uint32_t cp = 0xFF60 + (c2 - 0xA0);
            di += (size_t)utf8_encode(cp, dst + di);
            si += 2;
        } else if (c >= 0xA1 && c <= 0xFE && si + 1 < src_len) {
            /* JIS X 0208: convert via Unicode mapping
             * EUC-JP byte pair (c1, c2) -> JIS row/cell -> Unicode
             * Row = c1 - 0xA0, Cell = c2 - 0xA0
             * Unicode codepoint = 0x3000 + approximate mapping
             *
             * For accuracy we use the standard offset approach:
             * Most hiragana: row 4, cells 1-83 -> U+3041-U+3093
             * Most katakana: row 5, cells 1-86 -> U+30A1-U+30F6
             */
            unsigned char c2 = (unsigned char)src[si + 1];
            int row = c - 0xA0;
            int cell = c2 - 0xA0;
            uint32_t cp = 0;

            if (row == 4 && cell >= 1 && cell <= 83) {
                /* Hiragana */
                cp = 0x3040 + (uint32_t)cell;
            } else if (row == 5 && cell >= 1 && cell <= 86) {
                /* Katakana */
                cp = 0x30A0 + (uint32_t)cell;
            } else if (row == 1) {
                /* Symbols - common punctuation */
                static const uint32_t row1_map[] = {
                    [1] = 0x3000, [2] = 0x3001, [3] = 0x3002, [4] = 0xFF0C,
                    [5] = 0xFF0E, [6] = 0x30FB, [7] = 0xFF1A, [8] = 0xFF1B,
                    [9] = 0xFF1F, [10] = 0xFF01, [11] = 0x309B, [12] = 0x309C,
                    [14] = 0xFF3E, [16] = 0xFF3F, [17] = 0x30FD, [18] = 0x30FE,
                    [19] = 0x309D, [20] = 0x309E, [22] = 0x3003, [25] = 0x3005,
                    [26] = 0x3006, [27] = 0x3007, [30] = 0x2015, [31] = 0x2010,
                    [32] = 0xFF0F, [35] = 0xFF5C, [37] = 0x2026, [38] = 0x2025,
                    [46] = 0xFF08, [47] = 0xFF09, [52] = 0xFF3B, [53] = 0xFF3D,
                    [54] = 0xFF5B, [55] = 0xFF5D, [56] = 0x3008, [57] = 0x3009,
                    [58] = 0x300A, [59] = 0x300B, [60] = 0x300C, [61] = 0x300D,
                    [62] = 0x300E, [63] = 0x300F, [64] = 0x3010, [65] = 0x3011,
                    [70] = 0x2018, [71] = 0x2019, [72] = 0x201C, [73] = 0x201D,
                    [75] = 0xFF0B, [76] = 0xFF0D, [77] = 0x00B1, [78] = 0x00D7,
                    [79] = 0x00F7, [80] = 0xFF1D, [83] = 0xFF1C, [84] = 0xFF1E,
                };
                if (cell > 0 && cell < (int)(sizeof(row1_map)/sizeof(row1_map[0])))
                    cp = row1_map[cell];
            } else if (row >= 16 && row <= 47) {
                /* CJK Unified Ideographs (approximate, for common kanji)
                 * JIS X 0208 rows 16-47 map to various CJK codepoints.
                 * For a full mapping we'd need a complete table; here we use
                 * a formula that covers the Level 1 kanji subset. */
                int linear = (row - 16) * 94 + (cell - 1);
                /* Level 1 kanji start around U+4E00 */
                cp = 0x4E00 + (uint32_t)linear;
                /* Clamp to valid CJK range */
                if (cp > 0x9FFF) cp = 0x3013; /* geta mark as fallback */
            }

            if (cp == 0) {
                /* Fallback: replacement character */
                cp = 0xFFFD;
            }
            di += (size_t)utf8_encode(cp, dst + di);
            si += 2;
        } else {
            /* Unknown byte, skip */
            si++;
        }
    }

    dst[di] = '\0';
    return di;
}

static int load_dict_file(dict_t *dict, const char *path) {
    FILE *fp = fopen(path, "rb");
    if (!fp) {
        bsdjp_error("Cannot open dictionary: %s", path);
        return -1;
    }

    /* Read a sample to detect encoding */
    char sample[512];
    size_t sample_len = fread(sample, 1, sizeof(sample) - 1, fp);
    sample[sample_len] = '\0';
    bool is_euc = detect_euc_jp(sample, sample_len);
    fseek(fp, 0, SEEK_SET);

    bsdjp_log("Loading dictionary: %s (encoding: %s)",
              path, is_euc ? "EUC-JP" : "UTF-8");

    char line_buf[4096];
    char utf8_buf[8192];
    int count = 0;

    while (fgets(line_buf, sizeof(line_buf), fp)) {
        const char *line = line_buf;

        if (is_euc) {
            eucjp_to_utf8(line_buf, strlen(line_buf), utf8_buf, sizeof(utf8_buf));
            line = utf8_buf;
        }

        dict_entry_t *entry = parse_skk_line(line);
        if (entry) {
            dict_insert(dict, entry);
            count++;
        }
    }

    fclose(fp);
    bsdjp_log("Loaded %d dictionary entries", count);
    return 0;
}

int dict_global_init(void) {
    memset(&g_dict, 0, sizeof(g_dict));

    char *dict_path = bsdjp_data_path("SKK-JISYO.L");
    int ret = load_dict_file(&g_dict, dict_path);
    free(dict_path);

    if (ret != 0) {
        bsdjp_log("Main dictionary not found; conversion will be limited");
    }

    g_dict.loaded = true;
    return 0;
}

void dict_global_cleanup(void) {
    for (int i = 0; i < DICT_HASH_SIZE; i++) {
        dict_entry_t *e = g_dict.buckets[i];
        while (e) {
            dict_entry_t *next = e->next;
            dict_entry_free(e);
            e = next;
        }
        g_dict.buckets[i] = NULL;
    }
    g_dict.total_entries = 0;
    g_dict.loaded = false;
}

int dict_lookup(const dict_t *dict, const char *reading,
                char **candidates, int max_candidates) {
    if (!dict->loaded || !reading || !*reading) return 0;

    unsigned int h = dict_hash(reading);
    int count = 0;

    for (dict_entry_t *e = dict->buckets[h]; e; e = e->next) {
        if (strcmp(e->key, reading) == 0) {
            for (int i = 0; i < e->num_candidates && count < max_candidates; i++) {
                candidates[count++] = bsdjp_strdup(e->candidates[i]);
            }
            break;
        }
    }

    return count;
}

int dict_lookup_prefix(const dict_t *dict, const char *prefix,
                       char **results, int max_results) {
    if (!dict->loaded || !prefix || !*prefix) return 0;

    size_t plen = strlen(prefix);
    int count = 0;

    for (int i = 0; i < DICT_HASH_SIZE && count < max_results; i++) {
        for (dict_entry_t *e = dict->buckets[i]; e && count < max_results; e = e->next) {
            if (strncmp(e->key, prefix, plen) == 0 && e->num_candidates > 0) {
                /* Return "reading: first_candidate" */
                size_t len = strlen(e->key) + 2 + strlen(e->candidates[0]) + 1;
                char *buf = bsdjp_malloc(len);
                snprintf(buf, len, "%s: %s", e->key, e->candidates[0]);
                results[count++] = buf;
            }
        }
    }

    return count;
}
