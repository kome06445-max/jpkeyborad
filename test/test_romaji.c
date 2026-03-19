#include "romaji.h"
#include "util.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int tests_passed = 0;
static int tests_failed = 0;

#define ASSERT_EQ(desc, got, expected) do { \
    if (strcmp((got), (expected)) == 0) { \
        tests_passed++; \
        printf("  PASS: %s\n", (desc)); \
    } else { \
        tests_failed++; \
        printf("  FAIL: %s (got '%s', expected '%s')\n", \
               (desc), (got), (expected)); \
    } \
} while(0)

#define ASSERT_INT(desc, got, expected) do { \
    if ((got) == (expected)) { \
        tests_passed++; \
        printf("  PASS: %s\n", (desc)); \
    } else { \
        tests_failed++; \
        printf("  FAIL: %s (got %d, expected %d)\n", \
               (desc), (got), (expected)); \
    } \
} while(0)

static void test_basic_vowels(const romaji_table_t *t) {
    printf("=== Basic Vowels ===\n");
    char out[32];
    bool pending;

    int n = romaji_convert(t, "a", out, sizeof(out), false, &pending);
    ASSERT_INT("'a' consumed 1", n, 1);
    ASSERT_EQ("'a' -> あ", out, "あ");

    n = romaji_convert(t, "i", out, sizeof(out), false, &pending);
    ASSERT_EQ("'i' -> い", out, "い");

    n = romaji_convert(t, "u", out, sizeof(out), false, &pending);
    ASSERT_EQ("'u' -> う", out, "う");

    n = romaji_convert(t, "e", out, sizeof(out), false, &pending);
    ASSERT_EQ("'e' -> え", out, "え");

    n = romaji_convert(t, "o", out, sizeof(out), false, &pending);
    ASSERT_EQ("'o' -> お", out, "お");
}

static void test_consonant_vowel(const romaji_table_t *t) {
    printf("=== Consonant+Vowel ===\n");
    char out[32];
    bool pending;

    int n = romaji_convert(t, "ka", out, sizeof(out), false, &pending);
    ASSERT_INT("'ka' consumed 2", n, 2);
    ASSERT_EQ("'ka' -> か", out, "か");

    romaji_convert(t, "shi", out, sizeof(out), false, &pending);
    ASSERT_EQ("'shi' -> し", out, "し");

    romaji_convert(t, "tsu", out, sizeof(out), false, &pending);
    ASSERT_EQ("'tsu' -> つ", out, "つ");

    romaji_convert(t, "chi", out, sizeof(out), false, &pending);
    ASSERT_EQ("'chi' -> ち", out, "ち");

    romaji_convert(t, "fu", out, sizeof(out), false, &pending);
    ASSERT_EQ("'fu' -> ふ", out, "ふ");
}

static void test_youon(const romaji_table_t *t) {
    printf("=== Youon (Digraphs) ===\n");
    char out[32];
    bool pending;

    romaji_convert(t, "kya", out, sizeof(out), false, &pending);
    ASSERT_EQ("'kya' -> きゃ", out, "きゃ");

    romaji_convert(t, "sha", out, sizeof(out), false, &pending);
    ASSERT_EQ("'sha' -> しゃ", out, "しゃ");

    romaji_convert(t, "cho", out, sizeof(out), false, &pending);
    ASSERT_EQ("'cho' -> ちょ", out, "ちょ");

    romaji_convert(t, "nya", out, sizeof(out), false, &pending);
    ASSERT_EQ("'nya' -> にゃ", out, "にゃ");
}

static void test_katakana(const romaji_table_t *t) {
    printf("=== Katakana Mode ===\n");
    char out[32];
    bool pending;

    romaji_convert(t, "ka", out, sizeof(out), true, &pending);
    ASSERT_EQ("'ka' (kata) -> カ", out, "カ");

    romaji_convert(t, "shi", out, sizeof(out), true, &pending);
    ASSERT_EQ("'shi' (kata) -> シ", out, "シ");
}

static void test_pending(const romaji_table_t *t) {
    printf("=== Pending Detection ===\n");
    char out[32];
    bool pending;

    int n = romaji_convert(t, "k", out, sizeof(out), false, &pending);
    ASSERT_INT("'k' consumed 0 (pending)", n, 0);
    ASSERT_INT("'k' is pending", pending, 1);

    n = romaji_convert(t, "sh", out, sizeof(out), false, &pending);
    ASSERT_INT("'sh' consumed 0 (pending)", n, 0);
    ASSERT_INT("'sh' is pending", pending, 1);
}

static void test_nn(const romaji_table_t *t) {
    printf("=== N-row Special ===\n");
    char out[32];
    bool pending;

    romaji_convert(t, "nn", out, sizeof(out), false, &pending);
    ASSERT_EQ("'nn' -> ん", out, "ん");

    romaji_convert(t, "n'", out, sizeof(out), false, &pending);
    ASSERT_EQ("'n\\'' -> ん", out, "ん");
}

static void test_sokuon(void) {
    printf("=== Sokuon Detection ===\n");
    ASSERT_INT("'kk' is sokuon", romaji_is_sokuon('k', 'k'), 1);
    ASSERT_INT("'ss' is sokuon", romaji_is_sokuon('s', 's'), 1);
    ASSERT_INT("'tt' is sokuon", romaji_is_sokuon('t', 't'), 1);
    ASSERT_INT("'nn' is NOT sokuon", romaji_is_sokuon('n', 'n'), 0);
    ASSERT_INT("'aa' is NOT sokuon", romaji_is_sokuon('a', 'a'), 0);
    ASSERT_INT("'ks' is NOT sokuon", romaji_is_sokuon('k', 's'), 0);
}

static void test_small_kana(const romaji_table_t *t) {
    printf("=== Small Kana ===\n");
    char out[32];
    bool pending;

    romaji_convert(t, "xa", out, sizeof(out), false, &pending);
    ASSERT_EQ("'xa' -> ぁ", out, "ぁ");

    romaji_convert(t, "xtu", out, sizeof(out), false, &pending);
    ASSERT_EQ("'xtu' -> っ", out, "っ");
}

int main(void) {
    printf("BSDJP Romaji Conversion Tests\n");
    printf("==============================\n\n");

    char *path = bsdjp_data_path("romaji_table.txt");
    romaji_table_t table = {0};

    if (romaji_table_load(&table, path) != 0) {
        fprintf(stderr, "Failed to load romaji table from %s\n", path);
        free(path);
        return 1;
    }
    free(path);

    test_basic_vowels(&table);
    test_consonant_vowel(&table);
    test_youon(&table);
    test_katakana(&table);
    test_pending(&table);
    test_nn(&table);
    test_sokuon();
    test_small_kana(&table);

    printf("\n==============================\n");
    printf("Results: %d passed, %d failed\n", tests_passed, tests_failed);

    romaji_table_free(&table);
    return tests_failed > 0 ? 1 : 0;
}
