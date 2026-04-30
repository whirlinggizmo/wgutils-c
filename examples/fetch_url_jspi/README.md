# fetch_url JSPI example

This example demonstrates the wasm-side difference between:

- `fetch_url_async()` with JS-driven polling
- `fetch_url_sync()` with JSPI-enabled sync-like control flow

The example builds two browser outputs:

- `poll`: uses `fetch_url_async()` and `fetch_url_finish()`
- `jspi`: uses `fetch_url_sync()`

## Build

From the package root:

```bash
make fetch_url_examples
```

Built files are written to:

- `lib/examples/fetch_url_jspi/poll/`
- `lib/examples/fetch_url_jspi/jspi/`

## Serve

Serve the built output directory over HTTP and open one of:

- `.../poll/index.html`
- `.../jspi/index.html`

Example:

```bash
cd lib/examples/fetch_url_jspi
python3 -m http.server 4444
```

Then browse to:

- `http://localhost:4444/poll/index.html`
- `http://localhost:4444/jspi/index.html`

## What to look for

In the `poll` build:

- the export returns a plain number immediately
- JS must keep calling `example_poll_fetch()`
- the trace shows `poll:*` steps later

In the `jspi` build:

- the export returns a `Promise`
- the C flow stops at `fetch_url_sync()`
- once the fetch resolves, the trace continues after `fetch_url_sync()`
- no polling state machine is involved in the C call chain

## Emscripten details

The JSPI build uses:

- `-sJSPI=1`
- `-sJSPI_EXPORTS='["example_run_fetch"]'`

`-sASYNCIFY` is not required for this path.
