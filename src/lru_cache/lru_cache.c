#include "lru_cache.h"

#include <stdlib.h>
#include <string.h>

typedef struct lru_cache_entry
{
    char *key;
    unsigned char *data;
    size_t size;
    struct lru_cache_entry *prev;
    struct lru_cache_entry *next;
} lru_cache_entry_t;

struct lru_cache
{
    size_t max_bytes;
    size_t max_entries;
    size_t used_bytes;
    size_t entry_count;
    lru_cache_entry_t *head;
    lru_cache_entry_t *tail;
};

static lru_cache_entry_t *lru_cache_find(lru_cache_t *cache, const char *key)
{
    lru_cache_entry_t *entry = NULL;
    if (!cache || !key) {
        return NULL;
    }

    entry = cache->head;
    while (entry != NULL)
    {
        if (strcmp(entry->key, key) == 0) {
            return entry;
        }
        entry = entry->next;
    }

    return NULL;
}

static void lru_cache_remove_entry(lru_cache_t *cache, lru_cache_entry_t *entry)
{
    if (!cache || !entry) {
        return;
    }

    if (entry->prev) {
        entry->prev->next = entry->next;
    } else {
        cache->head = entry->next;
    }

    if (entry->next) {
        entry->next->prev = entry->prev;
    } else {
        cache->tail = entry->prev;
    }

    if (cache->used_bytes >= entry->size) {
        cache->used_bytes -= entry->size;
    } else {
        cache->used_bytes = 0;
    }

    if (cache->entry_count > 0) {
        cache->entry_count--;
    }

    free(entry->key);
    free(entry->data);
    free(entry);
}

static void lru_cache_touch_front(lru_cache_t *cache, lru_cache_entry_t *entry)
{
    if (!cache || !entry || cache->head == entry) {
        return;
    }

    if (entry->prev) {
        entry->prev->next = entry->next;
    }
    if (entry->next) {
        entry->next->prev = entry->prev;
    } else {
        cache->tail = entry->prev;
    }

    entry->prev = NULL;
    entry->next = cache->head;
    if (cache->head) {
        cache->head->prev = entry;
    }
    cache->head = entry;
    if (!cache->tail) {
        cache->tail = entry;
    }
}

static void lru_cache_evict_if_needed(lru_cache_t *cache)
{
    if (!cache) {
        return;
    }

    while ((cache->used_bytes > cache->max_bytes || cache->entry_count > cache->max_entries) &&
           cache->tail != NULL)
    {
        lru_cache_remove_entry(cache, cache->tail);
    }
}

lru_cache_t *lru_cache_create(size_t max_bytes, size_t max_entries)
{
    lru_cache_t *cache = NULL;
    if (max_bytes == 0 || max_entries == 0) {
        return NULL;
    }

    cache = (lru_cache_t *)calloc(1, sizeof(lru_cache_t));
    if (!cache) {
        return NULL;
    }

    cache->max_bytes = max_bytes;
    cache->max_entries = max_entries;
    return cache;
}

void lru_cache_destroy(lru_cache_t *cache)
{
    if (!cache) {
        return;
    }

    lru_cache_clear(cache);
    free(cache);
}

void lru_cache_clear(lru_cache_t *cache)
{
    lru_cache_entry_t *entry = NULL;
    lru_cache_entry_t *next = NULL;

    if (!cache) {
        return;
    }

    entry = cache->head;
    while (entry)
    {
        next = entry->next;
        free(entry->key);
        free(entry->data);
        free(entry);
        entry = next;
    }

    cache->head = NULL;
    cache->tail = NULL;
    cache->used_bytes = 0;
    cache->entry_count = 0;
}

bool lru_cache_get_copy(lru_cache_t *cache, const char *key, unsigned char **out_data, size_t *out_size)
{
    lru_cache_entry_t *entry = NULL;
    unsigned char *copy = NULL;

    if (!cache || !key || !out_data || !out_size) {
        return false;
    }

    *out_data = NULL;
    *out_size = 0;

    entry = lru_cache_find(cache, key);
    if (!entry || !entry->data || entry->size == 0) {
        return false;
    }

    copy = (unsigned char *)malloc(entry->size);
    if (!copy) {
        return false;
    }
    memcpy(copy, entry->data, entry->size);

    lru_cache_touch_front(cache, entry);
    *out_data = copy;
    *out_size = entry->size;
    return true;
}

int lru_cache_put_copy(lru_cache_t *cache, const char *key, const unsigned char *data, size_t size)
{
    lru_cache_entry_t *entry = NULL;
    unsigned char *data_copy = NULL;
    char *key_copy = NULL;

    if (!cache || !key || !data || size == 0) {
        return -1;
    }

    // Items larger than the whole cache budget are intentionally skipped.
    if (size > cache->max_bytes) {
        return -1;
    }

    entry = lru_cache_find(cache, key);
    if (entry)
    {
        data_copy = (unsigned char *)malloc(size);
        if (!data_copy) {
            return -1;
        }
        memcpy(data_copy, data, size);

        free(entry->data);
        entry->data = data_copy;
        if (cache->used_bytes >= entry->size) {
            cache->used_bytes -= entry->size;
        } else {
            cache->used_bytes = 0;
        }
        entry->size = size;
        cache->used_bytes += size;
        lru_cache_touch_front(cache, entry);
        lru_cache_evict_if_needed(cache);
        return 0;
    }

    entry = (lru_cache_entry_t *)calloc(1, sizeof(lru_cache_entry_t));
    if (!entry) {
        return -1;
    }

    key_copy = (char *)malloc(strlen(key) + 1);
    if (!key_copy) {
        free(entry);
        return -1;
    }
    strcpy(key_copy, key);

    data_copy = (unsigned char *)malloc(size);
    if (!data_copy) {
        free(key_copy);
        free(entry);
        return -1;
    }
    memcpy(data_copy, data, size);

    entry->key = key_copy;
    entry->data = data_copy;
    entry->size = size;
    entry->prev = NULL;
    entry->next = cache->head;

    if (cache->head) {
        cache->head->prev = entry;
    }
    cache->head = entry;
    if (!cache->tail) {
        cache->tail = entry;
    }

    cache->entry_count++;
    cache->used_bytes += size;
    lru_cache_evict_if_needed(cache);

    return 0;
}

size_t lru_cache_size_bytes(const lru_cache_t *cache)
{
    if (!cache) {
        return 0;
    }
    return cache->used_bytes;
}

size_t lru_cache_size_entries(const lru_cache_t *cache)
{
    if (!cache) {
        return 0;
    }
    return cache->entry_count;
}
