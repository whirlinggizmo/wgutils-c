# wgutils-c

`wgutils-c` is a small C utility library used by Whirling Gizmo projects. It builds for:

- desktop/native via `gcc` + `libcurl`
- web/wasm via Emscripten

The package produces:

- `lib/libwgutils.a`
- `lib/libwgutils.wasm.a`

## Build

```bash
make
```

Useful targets:

- `make desktop`
- `make wasm`
- `make headers`
- `make test_desktop`
- `make fetch_url_examples`
- `make fetch_url_sizes`

## Components

### `async/wg_op`

Minimal async operation state used by wasm-facing polling APIs.

- tracks `pending` vs `done`
- stores status code, opaque implementation data, and cleanup callback
- used by `fetch_url` and `fileio` async entrypoints

### `event`

Small event bus with named events.

- register persistent or one-shot listeners
- remove listeners individually or by event name
- emit payload pointers to all listeners

Public header: [src/event/event.h](src/event/event.h)

### `fetch_url`

HTTP fetch helpers with different behavior on desktop and wasm.

- `fetch_url_async()` starts a polling-style fetch operation
- `fetch_url_finish()` transfers the result out of the operation
- `fetch_url_sync()` performs a blocking/native fetch on desktop
- shared helper logic lives in `fetch_url_common.c/.h`

Wasm notes:

- `fetch_url_async()` remains the polling/callback-oriented API
- `fetch_url_sync()` is implemented using JSPI in the wasm build
- the wasm archive remains a single library: `lib/libwgutils.wasm.a`
- final wasm link targets that call `fetch_url_sync()` must be linked with `-sJSPI=1`
- any export that can reach `fetch_url_sync()` must be listed in `-sJSPI_EXPORTS=[...]`
- `-sASYNCIFY` is not required for `fetch_url_sync()`

Implementation files:

- shared helpers: [src/fetch_url/fetch_url_common.h](src/fetch_url/fetch_url_common.h)
- desktop: [src/fetch_url/fetch_url.c](src/fetch_url/fetch_url.c)
- wasm async path: [src/fetch_url/fetch_url.wasm.c](src/fetch_url/fetch_url.wasm.c)
- wasm sync/JSPI path: [src/fetch_url/fetch_url_sync.wasm.c](src/fetch_url/fetch_url_sync.wasm.c)

### `fileio`

Cross-platform file utilities with a shared common layer.

- init/deinit mount point state
- read/write files
- recursive mkdir and remove helpers
- wasm async restore/flush hooks for persistent storage sync

Implementation files:

- API: [src/fileio/fileio.h](src/fileio/fileio.h)
- shared filesystem helpers: [src/fileio/fileio_common.h](src/fileio/fileio_common.h)

### `json`

Thin wrapper around the vendored `cJSON` library.

- parse JSON text
- inspect object fields and arrays
- create and serialize JSON values

Public header: [src/json/json.h](src/json/json.h)

### `logger`

Wrapper around the vendored logger implementation.

- sets log level and quiet mode
- provides project-local log level names
- keeps vendor symbols out of most call sites

Public header: [src/logger/logger.h](src/logger/logger.h)

### `lru_cache`

In-memory LRU cache keyed by string.

- bounded by total bytes and entry count
- stores copies of payload data
- returns copied data to callers

Public header: [src/lru_cache/lru_cache.h](src/lru_cache/lru_cache.h)

### `path`

Path manipulation helpers.

- join paths
- normalize separators and relative segments
- test absoluteness
- get filename and extension
- allocate joined paths

Public header: [src/path/path.h](src/path/path.h)

### `uri`

URL/URI helpers.

- detect `scheme://...` URLs
- normalize only the URL path segment while preserving query/fragment

Public header: [src/uri/uri.h](src/uri/uri.h)

### `vec`

Small inline vector helpers layered on top of `raylib`/`raymath` types.

- mutate `Vector3` values in place
- scale, normalize, dot, cross, and length helpers
- requires `raylib` because the public API uses `Vector3`

Public header: [src/vec/vec3.h](src/vec/vec3.h)

### `websocket`

Cross-platform websocket client abstraction.

- callback-based open/close/error/message handling
- text and binary send helpers
- native and Emscripten-specific implementations

Public header: [src/websocket/websocket.h](src/websocket/websocket.h)

## Vendored dependencies

`wgutils-c` vendors several small libraries under `src/vendor/`:

- `cJSON`
- `cwalk`
- `ee`
- `list`
- `logger-c`

Desktop fetch support also depends on a locally built static `libcurl`, managed by the `Makefile`.

## Tests and examples

Desktop tests live under `tests/` and are run with:

```bash
make test_desktop
```

The browser/wasm fetch JSPI example lives under:

- [examples/fetch_url_jspi](examples/fetch_url_jspi)

Build it with:

```bash
make fetch_url_examples
```

This example exists to demonstrate the wasm-side difference between:

- polling fetch via `fetch_url_async()`
- sync-like JSPI fetch via `fetch_url_sync()`

The built browser outputs are written to:

- `lib/examples/fetch_url_jspi/poll/`
- `lib/examples/fetch_url_jspi/jspi/`
