#ifndef BSDJP_DICT_H
#define BSDJP_DICT_H

#include <stdbool.h>

#define DICT_MAX_CANDIDATES 64

/*
 * SKK dictionary entry.
 * SKK format: "かな /候補1/候補2/.../"
 * The key is the reading (hiragana), values are kanji candidates.
 */
typedef struct dict_entry {
    char *key;                          /* Reading (hiragana) */
    char *candidates[DICT_MAX_CANDIDATES];
    int   num_candidates;
    struct dict_entry *next;            /* Hash chain */
} dict_entry_t;

/* Hash table for dictionary */
#define DICT_HASH_SIZE 65536

typedef struct {
    dict_entry_t *buckets[DICT_HASH_SIZE];
    int total_entries;
    bool loaded;
} dict_t;

/* Global dictionary */
extern dict_t g_dict;

int  dict_global_init(void);
void dict_global_cleanup(void);

/* Lookup: returns candidates array and count */
int dict_lookup(const dict_t *dict, const char *reading,
                char **candidates, int max_candidates);

/* Prefix lookup for prediction */
int dict_lookup_prefix(const dict_t *dict, const char *prefix,
                       char **results, int max_results);

#endif /* BSDJP_DICT_H */
