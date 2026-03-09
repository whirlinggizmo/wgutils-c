#ifndef __LRU_CACHE_H__
#define __LRU_CACHE_H__

#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct lru_cache lru_cache_t;

lru_cache_t *lru_cache_create(size_t max_bytes, size_t max_entries);
void lru_cache_destroy(lru_cache_t *cache);
void lru_cache_clear(lru_cache_t *cache);

bool lru_cache_get_copy(lru_cache_t *cache, const char *key, unsigned char **out_data, size_t *out_size);
int lru_cache_put_copy(lru_cache_t *cache, const char *key, const unsigned char *data, size_t size);

size_t lru_cache_size_bytes(const lru_cache_t *cache);
size_t lru_cache_size_entries(const lru_cache_t *cache);

#ifdef __cplusplus
}
#endif

#endif // __LRU_CACHE_H__
