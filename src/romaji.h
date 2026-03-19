#ifndef BSDJP_ROMAJI_H
#define BSDJP_ROMAJI_H

#include <stdbool.h>
#include <stddef.h>

#define ROMAJI_MAX_INPUT   8
#define ROMAJI_MAX_OUTPUT  16

typedef struct {
    char romaji[ROMAJI_MAX_INPUT];
    char hiragana[ROMAJI_MAX_OUTPUT];
    char katakana[ROMAJI_MAX_OUTPUT];
} romaji_entry_t;

typedef struct {
    romaji_entry_t *entries;
    int count;
    int capacity;
} romaji_table_t;

/* Load romaji table from data file. Returns 0 on success. */
int  romaji_table_load(romaji_table_t *table, const char *path);
void romaji_table_free(romaji_table_t *table);

/*
 * Try to convert the romaji buffer.
 *   input:  null-terminated romaji string (e.g. "ka", "shi")
 *   output: filled with hiragana (if use_katakana=false) or katakana
 *   Returns: number of romaji chars consumed, or 0 if no match yet
 *   pending: set to true if input is a prefix of a longer entry
 */
int romaji_convert(const romaji_table_t *table,
                   const char *input,
                   char *output, size_t output_size,
                   bool use_katakana,
                   bool *pending);

/* Check if doubled consonant should trigger sokuon (っ) */
bool romaji_is_sokuon(char c1, char c2);

/* Global table instance */
extern romaji_table_t g_romaji_table;

#endif /* BSDJP_ROMAJI_H */
