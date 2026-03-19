#include "romaji.h"
#include "util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

romaji_table_t g_romaji_table = {0};

static int entry_cmp_by_length_desc(const void *a, const void *b) {
    const romaji_entry_t *ea = (const romaji_entry_t *)a;
    const romaji_entry_t *eb = (const romaji_entry_t *)b;
    int la = (int)strlen(ea->romaji);
    int lb = (int)strlen(eb->romaji);
    if (lb != la) return lb - la;
    return strcmp(ea->romaji, eb->romaji);
}

int romaji_table_load(romaji_table_t *table, const char *path) {
    FILE *fp = fopen(path, "r");
    if (!fp) {
        bsdjp_error("Cannot open romaji table: %s", path);
        return -1;
    }

    table->count = 0;
    table->capacity = 256;
    table->entries = bsdjp_malloc(table->capacity * sizeof(romaji_entry_t));

    char line[256];
    while (fgets(line, sizeof(line), fp)) {
        /* Skip comments and empty lines */
        if (line[0] == '#' || line[0] == '\n' || line[0] == '\r')
            continue;

        char romaji[ROMAJI_MAX_INPUT] = {0};
        char hiragana[ROMAJI_MAX_OUTPUT] = {0};
        char katakana[ROMAJI_MAX_OUTPUT] = {0};

        char *p = line;
        char *tab1 = strchr(p, '\t');
        if (!tab1) continue;
        size_t rlen = (size_t)(tab1 - p);
        if (rlen >= ROMAJI_MAX_INPUT) continue;
        memcpy(romaji, p, rlen);
        romaji[rlen] = '\0';

        p = tab1 + 1;
        char *tab2 = strchr(p, '\t');
        if (!tab2) continue;
        size_t hlen = (size_t)(tab2 - p);
        if (hlen >= ROMAJI_MAX_OUTPUT) continue;
        memcpy(hiragana, p, hlen);
        hiragana[hlen] = '\0';

        p = tab2 + 1;
        /* trim trailing whitespace */
        size_t klen = strlen(p);
        while (klen > 0 && (p[klen-1] == '\n' || p[klen-1] == '\r' || p[klen-1] == ' '))
            klen--;
        if (klen >= ROMAJI_MAX_OUTPUT) continue;
        memcpy(katakana, p, klen);
        katakana[klen] = '\0';

        if (table->count >= table->capacity) {
            table->capacity *= 2;
            table->entries = bsdjp_realloc(table->entries,
                                           table->capacity * sizeof(romaji_entry_t));
        }

        romaji_entry_t *e = &table->entries[table->count++];
        memcpy(e->romaji, romaji, ROMAJI_MAX_INPUT);
        memcpy(e->hiragana, hiragana, ROMAJI_MAX_OUTPUT);
        memcpy(e->katakana, katakana, ROMAJI_MAX_OUTPUT);
    }

    fclose(fp);

    /* Sort by romaji length descending for longest-match-first */
    qsort(table->entries, (size_t)table->count, sizeof(romaji_entry_t),
          entry_cmp_by_length_desc);

    bsdjp_log("Loaded %d romaji entries from %s", table->count, path);
    return 0;
}

void romaji_table_free(romaji_table_t *table) {
    free(table->entries);
    table->entries = NULL;
    table->count = 0;
    table->capacity = 0;
}

int romaji_convert(const romaji_table_t *table,
                   const char *input,
                   char *output, size_t output_size,
                   bool use_katakana,
                   bool *pending) {
    if (!input || !*input) return 0;

    size_t input_len = strlen(input);
    *pending = false;

    /* Try longest match first */
    for (int i = 0; i < table->count; i++) {
        const romaji_entry_t *e = &table->entries[i];
        size_t elen = strlen(e->romaji);

        if (elen == input_len && strcmp(e->romaji, input) == 0) {
            const char *kana = use_katakana ? e->katakana : e->hiragana;
            size_t klen = strlen(kana);
            if (klen < output_size) {
                memcpy(output, kana, klen + 1);
                return (int)elen;
            }
        }

        /* Check if input is a prefix of this entry (still pending) */
        if (elen > input_len && strncmp(e->romaji, input, input_len) == 0) {
            *pending = true;
        }
    }

    return 0;
}

static const char *sokuon_consonants = "bcdfghjklmpqrstvwxyz";

bool romaji_is_sokuon(char c1, char c2) {
    if (c1 != c2) return false;
    if (c1 == 'n' || c1 == 'a' || c1 == 'i' || c1 == 'u' ||
        c1 == 'e' || c1 == 'o')
        return false;
    return strchr(sokuon_consonants, tolower(c1)) != NULL;
}
