#include "lru_cache_test.h"

#include "lru_cache/lru_cache.h"
#include "test_common.h"

#include <string.h>

int test_lru_cache_run(void)
{
    lru_cache_t *cache = lru_cache_create(8, 2);
    unsigned char *copy = NULL;
    size_t copy_size = 0;
    const unsigned char a_data[] = { 'a', 'b', 'c' };
    const unsigned char b_data[] = { 'd', 'e' };
    const unsigned char c_data[] = { 'f', 'g', 'h', 'i' };
    const unsigned char a_new_data[] = { 'z', 'z' };
    const unsigned char huge_data[] = { '1', '2', '3', '4', '5', '6', '7', '8', '9' };

    TEST_ASSERT(cache != NULL);
    TEST_ASSERT(lru_cache_size_entries(cache) == 0);
    TEST_ASSERT(lru_cache_size_bytes(cache) == 0);

    TEST_ASSERT(lru_cache_put_copy(cache, "a", a_data, sizeof(a_data)) == 0);
    TEST_ASSERT(lru_cache_put_copy(cache, "b", b_data, sizeof(b_data)) == 0);
    TEST_ASSERT(lru_cache_size_entries(cache) == 2);
    TEST_ASSERT(lru_cache_size_bytes(cache) == 5);

    TEST_ASSERT(lru_cache_get_copy(cache, "a", &copy, &copy_size));
    TEST_ASSERT(copy != NULL);
    TEST_ASSERT(copy_size == sizeof(a_data));
    TEST_ASSERT(memcmp(copy, a_data, sizeof(a_data)) == 0);
    free(copy);

    TEST_ASSERT(lru_cache_put_copy(cache, "c", c_data, sizeof(c_data)) == 0);
    TEST_ASSERT(lru_cache_size_entries(cache) == 2);
    TEST_ASSERT(lru_cache_size_bytes(cache) == 7);

    TEST_ASSERT(!lru_cache_get_copy(cache, "b", &copy, &copy_size));
    TEST_ASSERT(lru_cache_get_copy(cache, "a", &copy, &copy_size));
    free(copy);
    TEST_ASSERT(lru_cache_get_copy(cache, "c", &copy, &copy_size));
    free(copy);

    TEST_ASSERT(lru_cache_put_copy(cache, "a", a_new_data, sizeof(a_new_data)) == 0);
    TEST_ASSERT(lru_cache_get_copy(cache, "a", &copy, &copy_size));
    TEST_ASSERT(copy_size == sizeof(a_new_data));
    TEST_ASSERT(memcmp(copy, a_new_data, sizeof(a_new_data)) == 0);
    free(copy);

    TEST_ASSERT(lru_cache_put_copy(cache, "too_big", huge_data, sizeof(huge_data)) == -1);

    lru_cache_destroy(cache);
    return 0;
}
