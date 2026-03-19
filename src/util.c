#include "util.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include <sys/stat.h>

int utf8_encode(uint32_t cp, char *out) {
    if (cp <= 0x7F) {
        out[0] = (char)cp;
        return 1;
    }
    if (cp <= 0x7FF) {
        out[0] = (char)(0xC0 | (cp >> 6));
        out[1] = (char)(0x80 | (cp & 0x3F));
        return 2;
    }
    if (cp <= 0xFFFF) {
        out[0] = (char)(0xE0 | (cp >> 12));
        out[1] = (char)(0x80 | ((cp >> 6) & 0x3F));
        out[2] = (char)(0x80 | (cp & 0x3F));
        return 3;
    }
    if (cp <= 0x10FFFF) {
        out[0] = (char)(0xF0 | (cp >> 18));
        out[1] = (char)(0x80 | ((cp >> 12) & 0x3F));
        out[2] = (char)(0x80 | ((cp >> 6) & 0x3F));
        out[3] = (char)(0x80 | (cp & 0x3F));
        return 4;
    }
    return 0;
}

int utf8_decode(const char *s, uint32_t *codepoint) {
    const unsigned char *p = (const unsigned char *)s;
    if (!p || !*p) return 0;

    if (p[0] < 0x80) {
        *codepoint = p[0];
        return 1;
    }
    if ((p[0] & 0xE0) == 0xC0) {
        *codepoint = ((uint32_t)(p[0] & 0x1F) << 6) |
                      (uint32_t)(p[1] & 0x3F);
        return 2;
    }
    if ((p[0] & 0xF0) == 0xE0) {
        *codepoint = ((uint32_t)(p[0] & 0x0F) << 12) |
                     ((uint32_t)(p[1] & 0x3F) << 6) |
                      (uint32_t)(p[2] & 0x3F);
        return 3;
    }
    if ((p[0] & 0xF8) == 0xF0) {
        *codepoint = ((uint32_t)(p[0] & 0x07) << 18) |
                     ((uint32_t)(p[1] & 0x3F) << 12) |
                     ((uint32_t)(p[2] & 0x3F) << 6) |
                      (uint32_t)(p[3] & 0x3F);
        return 4;
    }
    *codepoint = 0xFFFD;
    return 1;
}

size_t utf8_strlen(const char *s) {
    size_t len = 0;
    while (*s) {
        if ((*s & 0xC0) != 0x80)
            len++;
        s++;
    }
    return len;
}

char *bsdjp_strdup(const char *s) {
    if (!s) return NULL;
    size_t len = strlen(s);
    char *d = malloc(len + 1);
    if (d) memcpy(d, s, len + 1);
    return d;
}

char *bsdjp_strndup(const char *s, size_t n) {
    if (!s) return NULL;
    size_t len = strlen(s);
    if (n < len) len = n;
    char *d = malloc(len + 1);
    if (d) {
        memcpy(d, s, len);
        d[len] = '\0';
    }
    return d;
}

void bsdjp_str_trim(char *s) {
    if (!s) return;
    char *start = s;
    while (isspace((unsigned char)*start)) start++;
    if (start != s)
        memmove(s, start, strlen(start) + 1);
    char *end = s + strlen(s) - 1;
    while (end > s && isspace((unsigned char)*end))
        *end-- = '\0';
}

static bool file_exists(const char *path) {
    struct stat st;
    return stat(path, &st) == 0;
}

char *bsdjp_data_path(const char *filename) {
    char buf[1024];

#ifdef BSDJP_SOURCE_DATA_DIR
    snprintf(buf, sizeof(buf), "%s/%s", BSDJP_SOURCE_DATA_DIR, filename);
    if (file_exists(buf))
        return bsdjp_strdup(buf);
#endif

#ifdef BSDJP_DATA_DIR
    snprintf(buf, sizeof(buf), "%s/%s", BSDJP_DATA_DIR, filename);
    if (file_exists(buf))
        return bsdjp_strdup(buf);
#endif

    snprintf(buf, sizeof(buf), "data/%s", filename);
    if (file_exists(buf))
        return bsdjp_strdup(buf);

    snprintf(buf, sizeof(buf), "/usr/local/share/bsdjp/%s", filename);
    return bsdjp_strdup(buf);
}

void bsdjp_log(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    fprintf(stderr, "[bsdjp] ");
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    va_end(ap);
}

void bsdjp_error(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    fprintf(stderr, "[bsdjp ERROR] ");
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    va_end(ap);
}

void *bsdjp_malloc(size_t size) {
    void *p = malloc(size);
    if (!p) {
        bsdjp_error("Out of memory (requested %zu bytes)", size);
        abort();
    }
    return p;
}

void *bsdjp_calloc(size_t count, size_t size) {
    void *p = calloc(count, size);
    if (!p) {
        bsdjp_error("Out of memory (calloc %zu * %zu)", count, size);
        abort();
    }
    return p;
}

void *bsdjp_realloc(void *ptr, size_t size) {
    void *p = realloc(ptr, size);
    if (!p && size > 0) {
        bsdjp_error("Out of memory (realloc %zu bytes)", size);
        abort();
    }
    return p;
}
