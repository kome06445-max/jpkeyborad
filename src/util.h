#ifndef BSDJP_UTIL_H
#define BSDJP_UTIL_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

/* UTF-8 encoding helpers */
int utf8_encode(uint32_t codepoint, char *out);
int utf8_decode(const char *s, uint32_t *codepoint);
size_t utf8_strlen(const char *s);

/* String helpers */
char *bsdjp_strdup(const char *s);
char *bsdjp_strndup(const char *s, size_t n);
void  bsdjp_str_trim(char *s);

/* Path resolution: returns allocated path to data file */
char *bsdjp_data_path(const char *filename);

/* Logging */
void bsdjp_log(const char *fmt, ...);
void bsdjp_error(const char *fmt, ...);

/* Memory */
void *bsdjp_malloc(size_t size);
void *bsdjp_calloc(size_t count, size_t size);
void *bsdjp_realloc(void *ptr, size_t size);

#endif /* BSDJP_UTIL_H */
