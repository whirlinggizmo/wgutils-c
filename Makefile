WGUTILS_DIR := $(abspath .)
WGUTILS_SRC_DIR := $(WGUTILS_DIR)/src
ROOT_DIR := $(abspath ../..)
TEST_DIR := $(WGUTILS_DIR)/tests

CC ?= gcc
CC_WASM ?= emcc
AR ?= ar
EMAR ?= emar
EM_CACHE_DIR ?= $(abspath $(ROOT_DIR)/.emcache)
EM_ENV := EM_CACHE=$(EM_CACHE_DIR)

CFLAGS ?= -Wall -Wextra -fdiagnostics-color=always -O2
WASM_PLATFORM_CFLAGS ?= -DPLATFORM_WEB
WASM_COMMON_LDFLAGS ?= \
	-s WASM=1 \
	-lidbfs.js \
	-lwebsocket.js \
	-s FETCH=1 \
	-s EXPORT_ES6=1 \
	-s MODULARIZE=1 \
	-s ALLOW_MEMORY_GROWTH=1

INCLUDES := -I$(WGUTILS_SRC_DIR) -I$(WGUTILS_DIR) -I$(ROOT_DIR)/include -I$(ROOT_DIR)/deps/libraylib/include -I$(ROOT_DIR)/src -I$(TEST_DIR)

OUT_DIR := $(WGUTILS_DIR)/lib
INCLUDE_OUT_DIR := $(WGUTILS_DIR)/include
OBJ_DIR := $(WGUTILS_DIR)/obj
OBJ_DESKTOP_DIR := $(OBJ_DIR)/desktop
OBJ_WASM_DIR := $(OBJ_DIR)/wasm
DESKTOP_LIB := $(OUT_DIR)/libwgutils.a
WASM_LIB := $(OUT_DIR)/libwgutils.wasm.a

PUBLIC_HEADERS := \
	$(WGUTILS_SRC_DIR)/async/wg_op.h \
	$(WGUTILS_SRC_DIR)/event/event.h \
	$(WGUTILS_SRC_DIR)/fetch_url/fetch_url.h \
	$(WGUTILS_SRC_DIR)/fileio/fileio.h \
	$(WGUTILS_SRC_DIR)/fileio/fileio_common.h \
	$(WGUTILS_SRC_DIR)/logger/logger.h \
	$(WGUTILS_SRC_DIR)/lru_cache/lru_cache.h \
	$(WGUTILS_SRC_DIR)/path/path.h \
	$(WGUTILS_SRC_DIR)/uri/uri.h \
	$(WGUTILS_SRC_DIR)/vec/vec3.h \
	$(WGUTILS_SRC_DIR)/vendor/cjson/cJSON.h \
	$(WGUTILS_SRC_DIR)/websocket/websocket.h

PUBLIC_HEADER_OUTPUTS := $(patsubst $(WGUTILS_SRC_DIR)/%,$(INCLUDE_OUT_DIR)/%,$(PUBLIC_HEADERS))

DESKTOP_SRCS := \
	$(WGUTILS_SRC_DIR)/async/wg_op.c \
	$(WGUTILS_SRC_DIR)/event/event.c \
	$(WGUTILS_SRC_DIR)/fetch_url/fetch_url.c \
	$(WGUTILS_SRC_DIR)/fileio/fileio.c \
	$(WGUTILS_SRC_DIR)/fileio/fileio_common.c \
	$(WGUTILS_SRC_DIR)/logger/logger.c \
	$(WGUTILS_SRC_DIR)/lru_cache/lru_cache.c \
	$(WGUTILS_SRC_DIR)/path/path.c \
	$(WGUTILS_SRC_DIR)/uri/uri.c \
	$(WGUTILS_SRC_DIR)/vendor/cjson/cJSON.c \
	$(WGUTILS_SRC_DIR)/vendor/cwalk/cwalk.c \
	$(WGUTILS_SRC_DIR)/vendor/ee/ee.c \
	$(WGUTILS_SRC_DIR)/vendor/list/list.c \
	$(WGUTILS_SRC_DIR)/vendor/list/list_node.c \
	$(WGUTILS_SRC_DIR)/vendor/list/list_iterator.c \
	$(WGUTILS_SRC_DIR)/vendor/logger-c/log.c \
	$(WGUTILS_SRC_DIR)/websocket/websocket.c

WASM_SRCS := \
	$(WGUTILS_SRC_DIR)/async/wg_op.c \
	$(WGUTILS_SRC_DIR)/event/event.c \
	$(WGUTILS_SRC_DIR)/fetch_url/fetch_url.wasm.c \
	$(WGUTILS_SRC_DIR)/fileio/fileio.wasm.c \
	$(WGUTILS_SRC_DIR)/fileio/fileio_common.c \
	$(WGUTILS_SRC_DIR)/logger/logger.c \
	$(WGUTILS_SRC_DIR)/lru_cache/lru_cache.c \
	$(WGUTILS_SRC_DIR)/path/path.c \
	$(WGUTILS_SRC_DIR)/uri/uri.c \
	$(WGUTILS_SRC_DIR)/vendor/cjson/cJSON.c \
	$(WGUTILS_SRC_DIR)/vendor/cwalk/cwalk.c \
	$(WGUTILS_SRC_DIR)/vendor/ee/ee.c \
	$(WGUTILS_SRC_DIR)/vendor/list/list.c \
	$(WGUTILS_SRC_DIR)/vendor/list/list_node.c \
	$(WGUTILS_SRC_DIR)/vendor/list/list_iterator.c \
	$(WGUTILS_SRC_DIR)/vendor/logger-c/log.c \
	$(WGUTILS_SRC_DIR)/websocket/websocket.wasm.c

DESKTOP_OBJS := $(patsubst $(WGUTILS_SRC_DIR)/%.c,$(OBJ_DESKTOP_DIR)/%.o,$(DESKTOP_SRCS))
WASM_OBJS := $(patsubst $(WGUTILS_SRC_DIR)/%.c,$(OBJ_WASM_DIR)/%.o,$(WASM_SRCS))

TEST_BIN := /tmp/wgutils_unit_tests
URI_TEST_BIN := /tmp/wgutils_uri_test

UNIT_SRCS := \
	$(TEST_DIR)/tests_main.c \
	$(TEST_DIR)/fetch_url_stub.c \
	$(TEST_DIR)/event_test.c \
	$(TEST_DIR)/fileio_test.c \
	$(TEST_DIR)/lru_cache_test.c \
	$(TEST_DIR)/path_test.c \
	$(WGUTILS_SRC_DIR)/async/wg_op.c \
	$(WGUTILS_SRC_DIR)/event/event.c \
	$(WGUTILS_SRC_DIR)/fileio/fileio.c \
	$(WGUTILS_SRC_DIR)/fileio/fileio_common.c \
	$(WGUTILS_SRC_DIR)/logger/logger.c \
	$(WGUTILS_SRC_DIR)/lru_cache/lru_cache.c \
	$(WGUTILS_SRC_DIR)/path/path.c \
	$(WGUTILS_SRC_DIR)/uri/uri.c \
	$(WGUTILS_SRC_DIR)/vendor/cwalk/cwalk.c \
	$(WGUTILS_SRC_DIR)/vendor/ee/ee.c \
	$(WGUTILS_SRC_DIR)/vendor/list/list.c \
	$(WGUTILS_SRC_DIR)/vendor/list/list_node.c \
	$(WGUTILS_SRC_DIR)/vendor/list/list_iterator.c \
	$(WGUTILS_SRC_DIR)/vendor/logger-c/log.c

.PHONY: all deps desktop wasm clean test test_desktop unit_test uri_test headers

all: desktop wasm

deps:
	@echo "wgutils currently uses the system libcurl on desktop."

desktop: $(DESKTOP_LIB) headers
	@echo "Built desktop wgutils archive: $(DESKTOP_LIB)"

wasm: $(WASM_LIB) headers
	@echo "Built wasm wgutils archive: $(WASM_LIB)"

headers: $(PUBLIC_HEADER_OUTPUTS)
	@echo "Built wgutils include tree: $(INCLUDE_OUT_DIR)"

$(OBJ_DESKTOP_DIR)/%.o: $(WGUTILS_SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

$(OBJ_WASM_DIR)/%.o: $(WGUTILS_SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	@mkdir -p "$(EM_CACHE_DIR)"
	$(EM_ENV) $(CC_WASM) $(WASM_PLATFORM_CFLAGS) $(CFLAGS) $(INCLUDES) -c $< -o $@

$(DESKTOP_LIB): $(DESKTOP_OBJS)
	@mkdir -p $(OUT_DIR)
	$(AR) rcs $@ $(DESKTOP_OBJS)

$(WASM_LIB): $(WASM_OBJS)
	@mkdir -p $(OUT_DIR)
	@mkdir -p "$(EM_CACHE_DIR)"
	$(EM_ENV) $(EMAR) rcs $@ $(WASM_OBJS)

$(INCLUDE_OUT_DIR)/%: $(WGUTILS_SRC_DIR)/%
	@mkdir -p $(dir $@)
	cp $< $@

test: test_desktop

test_desktop: unit_test uri_test
	@echo "wgutils desktop tests passed."

unit_test:
	$(CC) $(CFLAGS) $(INCLUDES) $(UNIT_SRCS) -o $(TEST_BIN)
	@$(TEST_BIN) > /tmp/wgutils_unit_tests.log 2>&1 || (cat /tmp/wgutils_unit_tests.log && exit 1)
	@tail -n 1 /tmp/wgutils_unit_tests.log

uri_test: desktop
	$(CC) $(CFLAGS) $(INCLUDES) $(TEST_DIR)/uri_test.c $(DESKTOP_LIB) -lcurl -o $(URI_TEST_BIN)
	@$(URI_TEST_BIN)

clean:
	rm -rf $(OUT_DIR) $(INCLUDE_OUT_DIR) $(OBJ_DIR)
