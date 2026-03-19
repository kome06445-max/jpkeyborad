#include "user_dict.h"
#include "util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define USER_DICT_MAX_ENTRIES 16384
#define USER_DICT_DIR_NAME ".bsdjp"
#define USER_DICT_FILE_NAME "user_dict.txt"

typedef struct {
    char *reading;
    char *candidate;
    int   frequency;
} user_entry_t;

static user_entry_t *entries = NULL;
static int entry_count = 0;
static int entry_capacity = 0;
static char *dict_path = NULL;

static char *get_user_dict_path(void) {
    const char *home = getenv("HOME");
    if (!home) home = "/tmp";

    size_t len = strlen(home) + sizeof("/" USER_DICT_DIR_NAME "/" USER_DICT_FILE_NAME) + 1;
    char *path = bsdjp_malloc(len);
    snprintf(path, len, "%s/%s", home, USER_DICT_DIR_NAME);

    struct stat st;
    if (stat(path, &st) != 0) {
#ifdef _WIN32
        mkdir(path);
#else
        mkdir(path, 0700);
#endif
    }

    snprintf(path, len, "%s/%s/%s", home, USER_DICT_DIR_NAME, USER_DICT_FILE_NAME);
    return path;
}

static void load_user_dict(void) {
    FILE *fp = fopen(dict_path, "r");
    if (!fp) return;

    char line[2048];
    while (fgets(line, sizeof(line), fp)) {
        if (line[0] == '#' || line[0] == '\n') continue;

        char *tab1 = strchr(line, '\t');
        if (!tab1) continue;
        char *tab2 = strchr(tab1 + 1, '\t');
        if (!tab2) continue;

        *tab1 = '\0';
        *tab2 = '\0';

        char *reading = line;
        char *candidate = tab1 + 1;
        int freq = atoi(tab2 + 1);
        if (freq <= 0) freq = 1;

        if (entry_count >= entry_capacity) {
            entry_capacity = entry_capacity ? entry_capacity * 2 : 256;
            entries = bsdjp_realloc(entries, (size_t)entry_capacity * sizeof(user_entry_t));
        }

        entries[entry_count].reading = bsdjp_strdup(reading);
        entries[entry_count].candidate = bsdjp_strdup(candidate);
        entries[entry_count].frequency = freq;
        entry_count++;
    }

    fclose(fp);
    bsdjp_log("Loaded %d user dictionary entries", entry_count);
}

static void save_user_dict(void) {
    if (!dict_path) return;

    FILE *fp = fopen(dict_path, "w");
    if (!fp) {
        bsdjp_error("Cannot save user dictionary to %s", dict_path);
        return;
    }

    fprintf(fp, "# BSDJP User Dictionary\n");
    for (int i = 0; i < entry_count; i++) {
        fprintf(fp, "%s\t%s\t%d\n",
                entries[i].reading,
                entries[i].candidate,
                entries[i].frequency);
    }

    fclose(fp);
    bsdjp_log("Saved %d user dictionary entries", entry_count);
}

void user_dict_init(void) {
    dict_path = get_user_dict_path();
    entry_count = 0;
    entry_capacity = 256;
    entries = bsdjp_calloc((size_t)entry_capacity, sizeof(user_entry_t));
    load_user_dict();
}

void user_dict_cleanup(void) {
    save_user_dict();
    for (int i = 0; i < entry_count; i++) {
        free(entries[i].reading);
        free(entries[i].candidate);
    }
    free(entries);
    entries = NULL;
    entry_count = 0;
    entry_capacity = 0;
    free(dict_path);
    dict_path = NULL;
}

void user_dict_record(const char *reading, const char *candidate) {
    if (!reading || !candidate) return;

    /* Check if entry already exists */
    for (int i = 0; i < entry_count; i++) {
        if (strcmp(entries[i].reading, reading) == 0 &&
            strcmp(entries[i].candidate, candidate) == 0) {
            entries[i].frequency++;
            return;
        }
    }

    /* Add new entry */
    if (entry_count >= USER_DICT_MAX_ENTRIES) {
        /* Evict lowest frequency entry */
        int min_idx = 0;
        for (int i = 1; i < entry_count; i++) {
            if (entries[i].frequency < entries[min_idx].frequency)
                min_idx = i;
        }
        free(entries[min_idx].reading);
        free(entries[min_idx].candidate);
        entries[min_idx] = entries[entry_count - 1];
        entry_count--;
    }

    if (entry_count >= entry_capacity) {
        entry_capacity *= 2;
        entries = bsdjp_realloc(entries, (size_t)entry_capacity * sizeof(user_entry_t));
    }

    entries[entry_count].reading = bsdjp_strdup(reading);
    entries[entry_count].candidate = bsdjp_strdup(candidate);
    entries[entry_count].frequency = 1;
    entry_count++;
}

static int cmp_by_frequency_desc(const void *a, const void *b) {
    const user_entry_t *ea = (const user_entry_t *)a;
    const user_entry_t *eb = (const user_entry_t *)b;
    return eb->frequency - ea->frequency;
}

int user_dict_lookup(const char *reading, char **candidates, int max) {
    if (!reading || !*reading) return 0;

    /* Collect matching entries */
    user_entry_t *matches = NULL;
    int match_count = 0;

    for (int i = 0; i < entry_count; i++) {
        if (strcmp(entries[i].reading, reading) == 0) {
            match_count++;
        }
    }

    if (match_count == 0) return 0;

    matches = bsdjp_malloc((size_t)match_count * sizeof(user_entry_t));
    int idx = 0;
    for (int i = 0; i < entry_count && idx < match_count; i++) {
        if (strcmp(entries[i].reading, reading) == 0) {
            matches[idx++] = entries[i];
        }
    }

    qsort(matches, (size_t)match_count, sizeof(user_entry_t),
          cmp_by_frequency_desc);

    int result = 0;
    for (int i = 0; i < match_count && result < max; i++) {
        candidates[result++] = bsdjp_strdup(matches[i].candidate);
    }

    free(matches);
    return result;
}
