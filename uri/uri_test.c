#include "uri.h"

#include <stdio.h>
#include <string.h>

typedef struct
{
    const char *input;
    const char *expected;
} uri_case_t;

static int run_case(const uri_case_t *tc)
{
    char out[512] = {0};
    size_t out_len = uri_normalize(tc->input, out, sizeof(out));
    size_t expected_len = strlen(tc->expected);

    if (out_len != expected_len || strcmp(out, tc->expected) != 0) {
        fprintf(stderr, "FAIL\n");
        fprintf(stderr, "  input:    %s\n", tc->input);
        fprintf(stderr, "  expected: %s (len=%zu)\n", tc->expected, expected_len);
        fprintf(stderr, "  got:      %s (len=%zu)\n", out, out_len);
        return 1;
    }

    return 0;
}

static int run_truncation_case(void)
{
    const char *input = "https://localhost:4444/a/b/../c.ttf";
    char out[12] = {0};
    size_t len = uri_normalize(input, out, sizeof(out));
    if (len == 0 || out[sizeof(out) - 1] != '\0') {
        fprintf(stderr, "FAIL truncation/null-termination\n");
        return 1;
    }
    return 0;
}

int main(void)
{
    int failed = 0;
    const uri_case_t cases[] = {
        {"https://localhost:4444/a/b/../c.ttf", "https://localhost:4444/a/c.ttf"},
        {"https://localhost:4444//a///b/./c/../d.png?x=1#frag", "https://localhost:4444/a/b/d.png?x=1#frag"},
        {"https://localhost:4444", "https://localhost:4444"},
        {"https://localhost:4444?x=1", "https://localhost:4444?x=1"},
        {"https://localhost:4444#frag", "https://localhost:4444#frag"},
        {"https://localhost:4444/../up", "https://localhost:4444/up"},
        {"https://LOCALHOST:4444/a/%2E%2E/b", "https://LOCALHOST:4444/a/%2E%2E/b"},
        {"assets/../fonts/./KOMIKAH_.ttf", "assets/../fonts/./KOMIKAH_.ttf"},
    };

    if (!uri_is_url("https://localhost:4444/a.ttf")) {
        fprintf(stderr, "FAIL uri_is_url https\n");
        failed++;
    }
    if (!uri_is_url("custom+v1://host/path")) {
        fprintf(stderr, "FAIL uri_is_url custom scheme\n");
        failed++;
    }
    if (uri_is_url("assets/fonts/KOMIKAH_.ttf")) {
        fprintf(stderr, "FAIL uri_is_url non-url\n");
        failed++;
    }

    for (size_t i = 0; i < (sizeof(cases) / sizeof(cases[0])); i++) {
        failed += run_case(&cases[i]);
    }

    failed += run_truncation_case();

    if (failed == 0) {
        printf("uri_test: all tests passed (%zu cases)\n", sizeof(cases) / sizeof(cases[0]));
        return 0;
    }

    fprintf(stderr, "uri_test: %d failures\n", failed);
    return 1;
}
