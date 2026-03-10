#include "path_test.h"

#include "path/path.h"
#include "test_common.h"

#include <string.h>
#include <stdlib.h>

int test_path_run(void)
{
    char buf[512];
    size_t len = 0;
    char *joined = NULL;

    memset(buf, 0, sizeof(buf));
    len = path_join("a", "b", buf, sizeof(buf));
    TEST_ASSERT(strcmp(buf, "a/b") == 0);
    TEST_ASSERT(len == strlen("a/b"));

    memset(buf, 0, sizeof(buf));
    len = path_join("a", "b", buf, sizeof(buf), "c", "d");
    TEST_ASSERT(strcmp(buf, "a/b/c/d") == 0);
    TEST_ASSERT(len == strlen("a/b/c/d"));

    TEST_ASSERT(path_length("a", "bb", "ccc") == 8);

    memset(buf, 0, sizeof(buf));
    len = path_normalize("alpha/./beta/../gamma", buf, sizeof(buf));
    TEST_ASSERT(strcmp(buf, "alpha/gamma") == 0);
    TEST_ASSERT(len == strlen("alpha/gamma"));

    memset(buf, 0, sizeof(buf));
    len = path_normalize("https://EXAMPLE.com/a/../b/file.txt?x=1#frag", buf, sizeof(buf));
    TEST_ASSERT(strcmp(buf, "https://EXAMPLE.com/b/file.txt?x=1#frag") == 0);
    TEST_ASSERT(len == strlen("https://EXAMPLE.com/b/file.txt?x=1#frag"));

    TEST_ASSERT(strcmp(path_get_filename("/a/b/c.txt"), "c.txt") == 0);
    TEST_ASSERT(strcmp(path_get_filename("plain"), "plain") == 0);

    TEST_ASSERT(strcmp(path_get_extension("/a/b/c.txt"), "txt") == 0);
    TEST_ASSERT(path_get_extension("/a/b/noext") == NULL);

    joined = path_join_alloc("root", "dir", "file.txt");
    TEST_ASSERT(joined != NULL);
    TEST_ASSERT(strcmp(joined, "root/dir/file.txt") == 0);
    free(joined);

    return 0;
}
