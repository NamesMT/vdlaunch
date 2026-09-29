# AGENTS.md

Guardrails for working in this repo. Depth lives in [`docs/`](docs/README.md),
which is the source of truth when this file and the code disagree.

## Essentials

- Build: `make` → `dist/vdlaunch64.exe`, `vdlaunch32.exe`. `make test` builds and runs the suite.
- Three binaries: `vdlaunch64/32.exe` (the launcher) and `vdlaunchCreator.exe` (wrap-in-place tool).
- Launchers need **no helper files**: placement runs on explorer's side, so either bitness wraps either target. The creator embeds both launchers as RCDATA and picks by the target's PE machine.
- Layout: `src/*.cpp`, tests in `test/run.sh`, probes in `tools/`, depth in `docs/` (keys: [CONFIG.md](docs/CONFIG.md)).
- The engine's slot numbers live only in `src/desktop.cpp`; never duplicate them elsewhere.

## Windows COM (see [docs/COM.md](docs/COM.md))

- Never trust an `HRESULT` from the internal manager: a wrong slot **faults silently** or no-ops. Confirm by effect (desktop count, current desktop).
- Acquire the manager with `IServiceProvider::QueryService`, never `CoCreateInstance` (returns `E_NOINTERFACE`).
- Slots count `IUnknown`'s three entries, so the first own method is slot 3.
- Only build 26100/26200 is verified slot-by-slot. Older revisions vary in IID **and** layout, so unverified tables stay behind `experimental_layout`.
- Verify a new slot on real hardware before encoding it; a guessed slot crashes the wrapper, which is worse than refusing.

## The creator

- It renames a user's app, so keep the order: validate → stage the launcher → rename → write the ini → roll back on any failure.
- Match the `vdlaunch-wrapper` marker with `std::string::find`, never `strstr` (PE files are full of NULs).
- Refuse to wrap a wrapper, and refuse to clobber an existing `_name.exe` without `--force` — that backup is the user's real app.
- Blank prompt answers omit the key so the launcher default applies.

## Behaviour you must not regress

- Passthrough is byte-exact: never re-quote, reorder or re-encode the app's arguments.
- Default is a silent background launch (`switch = false`); the user must not be dragged to the target desktop.
- `desktop` is 1-based; missing indexes are created when `create = true`.
- A failure must degrade, not crash: log the reason, launch anyway, and keep `--diag`/`--print-config` honest.

## Building and testing (see [docs/BUILD-AND-TEST.md](docs/BUILD-AND-TEST.md))

- mingw-w64 cross-compile from Linux/WSL; `make console` adds stdout-capable twins used only by tests.
- `cmd.exe` cannot take a WSL working directory — always `cd /d C:\…` inside the call, and keep test files under `/mnt/c/`.
- GUI-subsystem exes do not block `cmd`, so use `start /wait` or a console twin when you need output.
- Prefer a `.bat` wrapper over chained `set "V=x"&& prog` one-liners; the trailing space corrupts the value.
- The suite drives the real host's desktops: restore the count before finishing, and set `quiet = true` so a refusal is not a modal hang.
- Mock builds live in `.mocks/` (gitignored); do not rely on `/tmp`, it does not survive.
- Test only what can fail: don't add assertions that pass whether or not the code works.

## Editing

- Minimal comments: only non-obvious intent, no narration.
- The Makefile tracks `src/*.h`; if a build still looks stale, `make clean`.
- When you learn a new COM constant or host gotcha, update `docs/` — an empty-context agent should not have to rediscover it.
