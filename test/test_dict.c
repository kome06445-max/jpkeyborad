#include "dict.h"
#include "util.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int tests_passed = 0;
static int tests_failed = 0;

#define ASSERT_TRUE(desc, cond) do { \
    if (cond) { \
        tests_passed++; \
        printf("  PASS: %s\n", (desc)); \
    } else { \
        tests_failed++; \
        printf("  FAIL: %s\n", (desc)); \
    } \
} while(0)

static void test_dict_load(void) {
    printf("=== Dictionary Load ===\n");
    ASSERT_TRUE("Dictionary marked as loaded", g_dict.loaded);
    printf("  INFO: Total entries loaded: %d\n", g_dict.total_entries);
}

static void test_dict_lookup(void) {
    printf("=== Dictionary Lookup ===\n");

    if (g_dict.total_entries == 0) {
        printf("  SKIP: No dictionary loaded (download SKK-JISYO.L first)\n");
        return;
    }

    /* Test a common word */
    char *cands[16];
    int count = dict_lookup(&g_dict, "にほん", cands, 16);
    printf("  INFO: 'にほん' -> %d candidates\n", count);
    ASSERT_TRUE("'にほん' has candidates", count > 0);

    for (int i = 0; i < count; i++) {
        printf("    [%d] %s\n", i, cands[i]);
        free(cands[i]);
    }

    /* Test another word */
    count = dict_lookup(&g_dict, "かんじ", cands, 16);
    printf("  INFO: 'かんじ' -> %d candidates\n", count);
    for (int i = 0; i < count; i++) {
        printf("    [%d] %s\n", i, cands[i]);
        free(cands[i]);
    }
}

static void test_dict_prefix(void) {
    printf("=== Prefix Lookup ===\n");

    if (g_dict.total_entries == 0) {
        printf("  SKIP: No dictionary loaded\n");
        return;
    }

    char *results[8];
    int count = dict_lookup_prefix(&g_dict, "にほ", results, 8);
    printf("  INFO: prefix 'にほ' -> %d matches\n", count);
    ASSERT_TRUE("Prefix 'にほ' has matches", count > 0);

    for (int i = 0; i < count; i++) {
        printf("    %s\n", results[i]);
        free(results[i]);
    }
}

static void test_empty_lookup(void) {
    printf("=== Edge Cases ===\n");

    char *cands[4];
    int count = dict_lookup(&g_dict, "", cands, 4);
    ASSERT_TRUE("Empty string returns 0", count == 0);

    count = dict_lookup(&g_dict, "zzzzzzzzz", cands, 4);
    ASSERT_TRUE("Nonexistent key returns 0", count == 0);
}

int main(void) {
    printf("BSDJP Dictionary Tests\n");
    printf("======================\n\n");

    int ret = dict_global_init();
    if (ret != 0) {
        printf("WARNING: Dictionary init returned error (file may not exist)\n");
    }

    test_dict_load();
    test_dict_lookup();
    test_dict_prefix();
    test_empty_lookup();

    printf("\n======================\n");
    printf("Results: %d passed, %d failed\n", tests_passed, tests_failed);

    dict_global_cleanup();
    return tests_failed > 0 ? 1 : 0;
}
