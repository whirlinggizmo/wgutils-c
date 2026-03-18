WGUTILS_DIR := $(abspath .)
WGUTILS_SRC_DIR := $(WGUTILS_DIR)/src
ROOT_DIR := $(abspath ../..)
TEST_DIR := $(WGUTILS_DIR)/tests
DEPS_DIR := $(WGUTILS_DIR)/deps
DEPS_BUILD_DIR := $(DEPS_DIR)/build

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

CURL_VERSION ?= 8.19.0
CURL_URL := https://curl.se/download/curl-$(CURL_VERSION).tar.xz
CURL_SRC_DIR := $(DEPS_DIR)/libcurl
CURL_BUILD_DIR := $(DEPS_BUILD_DIR)/libcurl
CURL_PREFIX := $(CURL_BUILD_DIR)/install
CURL_STATIC_LIB := $(CURL_PREFIX)/lib/libcurl.a
CURL_CONFIG := $(CURL_PREFIX)/bin/curl-config
CURL_CONFIGURE_ARGS := \
	--prefix=$(CURL_PREFIX) \
	--disable-shared \
	--enable-static \
	--with-openssl \
	--disable-ldap \
	--disable-ldaps \
	--disable-ftp \
	--disable-file \
	--disable-dict \
	--disable-telnet \
	--disable-tftp \
	--disable-pop3 \
	--disable-imap \
	--disable-smb \
	--disable-smtp \
	--disable-gopher \
	--disable-rtsp \
	--disable-manual \
	--disable-docs \
	--without-libpsl \
	--without-libidn2 \
	--without-brotli \
	--without-zstd

INCLUDES := -I$(WGUTILS_SRC_DIR) -I$(WGUTILS_DIR) -I$(ROOT_DIR)/include -I$(ROOT_DIR)/deps/libraylib/include -I$(ROOT_DIR)/src -I$(TEST_DIR)
DESKTOP_INCLUDES := $(INCLUDES) -I$(CURL_PREFIX)/include
WASM_INCLUDES := $(INCLUDES)

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
	$(WGUTILS_SRC_DIR)/json/json.h \
	$(WGUTILS_SRC_DIR)/logger/logger.h \
	$(WGUTILS_SRC_DIR)/lru_cache/lru_cache.h \
	$(WGUTILS_SRC_DIR)/path/path.h \
	$(WGUTILS_SRC_DIR)/uri/uri.h \
	$(WGUTILS_SRC_DIR)/vec/vec3.h \
	$(WGUTILS_SRC_DIR)/websocket/websocket.h

PUBLIC_HEADER_OUTPUTS := $(patsubst $(WGUTILS_SRC_DIR)/%,$(INCLUDE_OUT_DIR)/%,$(PUBLIC_HEADERS))

DESKTOP_SRCS := \
	$(WGUTILS_SRC_DIR)/async/wg_op.c \
	$(WGUTILS_SRC_DIR)/event/event.c \
	$(WGUTILS_SRC_DIR)/fetch_url/fetch_url.c \
	$(WGUTILS_SRC_DIR)/fileio/fileio.c \
	$(WGUTILS_SRC_DIR)/fileio/fileio_common.c \
	$(WGUTILS_SRC_DIR)/json/json.c \
	$(WGUTILS_SRC_DIR)/logger/logger.c \
	$(WGUTILS_SRC_DIR)/lru_cache/lru_cache.c \
	$(WGUTILS_SRC_DIR)/path/path.c \
	$(WGUTILS_SRC_DIR)/uri/uri.c \
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
	$(WGUTILS_SRC_DIR)/json/json.c \
	$(WGUTILS_SRC_DIR)/logger/logger.c \
	$(WGUTILS_SRC_DIR)/lru_cache/lru_cache.c \
	$(WGUTILS_SRC_DIR)/path/path.c \
	$(WGUTILS_SRC_DIR)/uri/uri.c \
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

.PHONY: all deps libcurl desktop wasm clean deep-clean test test_desktop unit_test uri_test headers header_stale_clean

all: desktop wasm

deps: libcurl

libcurl: $(CURL_STATIC_LIB)
	@echo "Built local libcurl: $(CURL_STATIC_LIB)"

desktop: $(DESKTOP_LIB) $(DESKTOP_LINK_FLAGS_FILE) headers
	@echo "Built desktop wgutils archive: $(DESKTOP_LIB)"

wasm: $(WASM_LIB) headers
	@echo "Built wasm wgutils archive: $(WASM_LIB)"

headers: header_stale_clean $(PUBLIC_HEADER_OUTPUTS)
	@echo "Built wgutils include tree: $(INCLUDE_OUT_DIR)"

header_stale_clean:
	rm -f $(INCLUDE_OUT_DIR)/vendor/cjson/cJSON.h
	rm -f $(INCLUDE_OUT_DIR)/json/cJSON.h
	rm -f $(INCLUDE_OUT_DIR)/json/json.h

$(CURL_SRC_DIR)/configure:
	rm -rf "$(CURL_SRC_DIR)"
	@mkdir -p "$(CURL_SRC_DIR)"
	@tmp_tar="$$(mktemp /tmp/wgutils-curl-$(CURL_VERSION)-XXXXXX.tar.xz)"; \
	curl -L "$(CURL_URL)" -o "$$tmp_tar"; \
	tar -xf "$$tmp_tar" --strip-components=1 -C "$(CURL_SRC_DIR)"; \
	rm -f "$$tmp_tar"

$(CURL_STATIC_LIB): $(CURL_SRC_DIR)/configure
	@mkdir -p "$(CURL_BUILD_DIR)"
	cd "$(CURL_BUILD_DIR)" && "$(CURL_SRC_DIR)/configure" $(CURL_CONFIGURE_ARGS)
	$(MAKE) -C "$(CURL_BUILD_DIR)"
	$(MAKE) -C "$(CURL_BUILD_DIR)" install

$(OBJ_DESKTOP_DIR)/%.o: $(WGUTILS_SRC_DIR)/%.c | $(CURL_STATIC_LIB)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(DESKTOP_INCLUDES) -c $< -o $@

$(OBJ_WASM_DIR)/%.o: $(WGUTILS_SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	@mkdir -p "$(EM_CACHE_DIR)"
	$(EM_ENV) $(CC_WASM) $(WASM_PLATFORM_CFLAGS) $(CFLAGS) $(WASM_INCLUDES) -c $< -o $@

$(DESKTOP_LIB): $(DESKTOP_OBJS) $(CURL_STATIC_LIB)
	@mkdir -p $(OUT_DIR)
	@mkdir -p $(OBJ_DESKTOP_DIR)/.curl_unpack
	@echo "[desktop] Unpacking libcurl.a into libwgutils.a..."
	cd $(OBJ_DESKTOP_DIR)/.curl_unpack && $(AR) x $(abspath $(CURL_STATIC_LIB))
	$(AR) rcs $@ $(DESKTOP_OBJS) $(OBJ_DESKTOP_DIR)/.curl_unpack/*.o
	@rm -rf $(OBJ_DESKTOP_DIR)/.curl_unpack

$(WASM_LIB): $(WASM_OBJS)
	@mkdir -p $(OUT_DIR)
	@mkdir -p "$(EM_CACHE_DIR)"
	$(EM_ENV) $(EMAR) rcs $@ $(WASM_OBJS)

$(INCLUDE_OUT_DIR)/%: $(WGUTILS_SRC_DIR)/%
	@mkdir -p $(dir $@)
	install -m 644 $< $@

test: test_desktop

test_desktop: unit_test uri_test
	@echo "wgutils desktop tests passed."

unit_test:
	$(CC) $(CFLAGS) $(INCLUDES) $(UNIT_SRCS) -o $(TEST_BIN)
	@$(TEST_BIN) > /tmp/wgutils_unit_tests.log 2>&1 || (cat /tmp/wgutils_unit_tests.log && exit 1)
	@tail -n 1 /tmp/wgutils_unit_tests.log

uri_test: desktop
	$(CC) $(CFLAGS) $(INCLUDES) $(TEST_DIR)/uri_test.c $(DESKTOP_LIB) $$(cat $(DESKTOP_LINK_FLAGS_FILE)) -o $(URI_TEST_BIN)
	@$(URI_TEST_BIN)

clean:
	rm -rf $(OUT_DIR) $(INCLUDE_OUT_DIR) $(OBJ_DIR)

deep-clean: clean
	rm -rf $(CURL_SRC_DIR) $(DEPS_BUILD_DIR)
