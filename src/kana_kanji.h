#ifndef BSDJP_KANA_KANJI_H
#define BSDJP_KANA_KANJI_H

#include <stddef.h>

#define KANA_KANJI_MAX_CANDIDATES 64

/*
 * Convert hiragana reading to kanji candidates.
 * Fills candidates array with allocated strings.
 * Returns number of candidates found.
 *
 * Strategy:
 *  1. Check user dictionary first (recently used entries)
 *  2. Look up exact match in SKK dictionary
 *  3. Try progressively shorter prefixes for compound word lookup
 *  4. Add the original hiragana and katakana as fallback candidates
 */
int kana_kanji_convert(const char *reading, char **candidates, int max);

/* Utility: convert hiragana string to katakana (defined in engine.c) */
void kana_to_katakana(const char *hiragana, char *out, size_t out_size);

#endif /* BSDJP_KANA_KANJI_H */
