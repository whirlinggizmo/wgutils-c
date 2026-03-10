WGUTILS_DIR := $(abspath .)
ROOT_DIR := $(abspath ../..)
TEST_DIR := $(WGUTILS_DIR)/tests

CC ?= gcc
CFLAGS ?= -Wall -Wextra -fdiagnostics-color=always -O2
INCLUDES := -I$(WGUTILS_DIR) -I$(ROOT_DIR)/include -I$(ROOT_DIR)/deps/libraylib/include -I$(ROOT_DIR)/src -I$(TEST_DIR)

TEST_BIN := /tmp/wgutils_unit_tests
URI_TEST_BIN := /tmp/wgutils_uri_test

UNIT_SRCS := \
	$(TEST_DIR)/tests_main.c \
	$(TEST_DIR)/fetch_url_stub.c \
	$(TEST_DIR)/fileio_test.c \
	$(TEST_DIR)/lru_cache_test.c \
	$(TEST_DIR)/path_test.c \
	$(WGUTILS_DIR)/fileio/fileio.c \
	$(WGUTILS_DIR)/fileio/fileio_common.c \
	$(WGUTILS_DIR)/logger/log.c \
	$(WGUTILS_DIR)/lru_cache/lru_cache.c \
	$(WGUTILS_DIR)/path/path.c \
	$(WGUTILS_DIR)/uri/uri.c \
	$(WGUTILS_DIR)/vendor/cwalk/cwalk.c \
	$(WGUTILS_DIR)/vendor/logger-c/log.c

.PHONY: test test_desktop unit_test uri_test

test: test_desktop

test_desktop: unit_test uri_test
	@echo "wgutils desktop tests passed."

unit_test:
	$(CC) $(CFLAGS) $(INCLUDES) $(UNIT_SRCS) -o $(TEST_BIN)
	@$(TEST_BIN) > /tmp/wgutils_unit_tests.log 2>&1 || (cat /tmp/wgutils_unit_tests.log && exit 1)
	@tail -n 1 /tmp/wgutils_unit_tests.log

uri_test:
	$(CC) $(CFLAGS) $(INCLUDES) $(TEST_DIR)/uri_test.c $(WGUTILS_DIR)/uri/uri.c $(WGUTILS_DIR)/vendor/cwalk/cwalk.c -o $(URI_TEST_BIN)
	@$(URI_TEST_BIN)
