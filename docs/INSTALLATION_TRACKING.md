# Installation tracking

The Play flow uses a single worker to fetch the private manifest, prepare Java and
the game, and validate the instance. Only the window starts Minecraft, after it
receives a successful result from the current attempt. A cancellation request
always wins over a simultaneous completion and prevents the automatic start.

## Contracts and states

`InstallationProgress` identifies the stage, activity and state, an optional
percentage, transferred bytes, an optional total, file counters and a transfer
identifier. Java and the game share the same callback. Results distinguish success,
cancellation and failure; categories include network, HTTP, disk, integrity,
process and configuration. The coordinator presents guidance and keeps the details
in the log.

`InstallationMailbox` keeps the latest sample, the state of the six stages and the
terminal result. Win32 messages only signal that data is available; they never
carry pointers that own memory. Each attempt has an increasing identifier.
Stages and results are published immediately; transfer samples are throttled to
200 ms and a 200 ms timer collects the pending sample.

The downloader checks the cache before building the queue. Complete, valid files
don't count toward the total to transfer or the bytes used for speed. Parallel
workers serialize their updates. The completed-files counter only advances after
hash/size verification and atomic publication; 100% requires the verified queue.

The meter uses `steady_clock`, a five-second moving window and samples from the
current transfer. The estimate appears after two seconds, with a known total and
positive speed. Two seconds without traffic clear the speed and the estimate.
Verification, extraction and the Forge run never get artificial estimates.

## Cancellation and recovery

WinHTTP runs asynchronously inside the worker, keeping buffers and context alive
until `HANDLE_CLOSING`. Metadata and files receive a `stop_token`; reads use data
availability to deliver small chunks promptly. Hashing checks the signal between
chunks. Tar and Forge share a wait that terminates the subprocess on cancellation.
Java activation and each file's publication keep their atomic boundaries; complete
files remain available.

Partial files are removed. Continue/try again reads the manifests and rechecks the
cache. There is no persistent checkpoint, pause or byte-level resume. Mod cleanup
checks for cancellation before each removal. When closed during preparation, the
window keeps processing messages until the terminal result, waits for the workers
and frees old payloads before the HWND is destroyed.

The attempt's UTF-8 log lives in `logs/installation.log`; Forge installer details
stay in `logs/forge-installer.log`. The flow never logs the Microsoft identity or
tokens and strips URLs/credential markers from the details.

## Verification

Reference build: MSVC Release `/W4` in `out/build/installation`.
The six CTest suites cover the AutoUpdate and existing regressions, metrics with a
controlled clock, coalescing and stale attempts, a local HTTP server, cache and
parallel downloads, unknown sizes, HTTP 503, interruption, cancellation during
metadata/body, integrity, disk and subprocess failures.

The coordinator test uses isolated services for a first preparation, an existing
instance, a validated custom Java, cancellation during the natives, Forge and
validation stages, and world preservation. The hidden Win32 host checks the buttons,
repeated clicks, a late result, success versus cancellation, a single start and
closing without blocking the window.

`WindowPaintingTests --capture-installation <folder>` opens a visible native test
host with real controls, confirms Tab moves from **Abrir log** (Open log) to
**Cancelar instalação** (Cancel installation) and Enter cancels, and captures the
client area with PrintWindow at 1180 × 730 and 944 × 584. It uses synthetic data
and never calls the network, login or the real Minecraft.

`EternalLauncher --render-preview <folder>` renders the verifying, downloading,
Forge, error, cancelled, cancelling and compact states with the production renderer.
The captures are in `.impeccable/review/installation`.

This validation did not include a full installation from the public services, a
real Forge run or starting the real Minecraft. Subprocess waiting was tested with
an isolated executable. Multi-monitor DPI, screen readers and the duration of a real
installation were not measured. No release was published.
