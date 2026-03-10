#include "lru_cache_test.h"
#include "path_test.h"

#include <stdio.h>

int fileio_test_run(void);

int main(void)
{
    int passed = 0;
    int failed = 0;
    int rc = 0;

    rc = fileio_test_run();
    if (rc == 0) {
        passed++;
    } else {
        failed++;
    }

    rc = test_lru_cache_run();
    if (rc == 0) {
        passed++;
    } else {
        failed++;
    }

    rc = test_path_run();
    if (rc == 0) {
        passed++;
    } else {
        failed++;
    }

    printf("wgutils_tests: %d passed, %d failed\n", passed, failed);
    return failed == 0 ? 0 : 1;
}
