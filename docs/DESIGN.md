# Design notes

## Modules

| File | Responsibility |
| --- | --- |
| `src/main.cpp` | Entry point. Routes `--help`/`--version`/`--print-config`/`--diag`/`--cleanup-desktops`, else launches |
| `src/switches.cpp` | Single-pass command-line split: our leading switches vs the app's tail |
| `src/config.cpp` | `vdlaunch.ini` → `Config`; `[apps.*]` rule matching; `%VAR%` expansion |
| `src/ini.cpp` | Minimal ini reader (sections, `key=value`, `;`/`#` comments) |
| `src/launch.cpp` | Builds the command line and environment, `CreateProcessW`, window placement |
| `src/desktop.cpp` | The whole COM engine — see [COM.md](COM.md) |
| `src/util.cpp` | Paths, UTF-8, logging, glob, `windows_build()` |

## Argument handling (the part most easily broken)

`parse_cli` walks the raw command line and the argv array **in lockstep** so it knows
the exact character offset where the app's arguments start. The substring from there is
the passthrough tail, used verbatim.

Rules:

- Our switches only count in leading position; the first unrecognised token ends parsing.
- The tail is passed **byte-exact**. Never re-quote, reorder, or re-encode it.
- `argv[0]` is replaced with the real target path, so the app sees itself as itself.
- Tokenizing mirrors the CRT: quotes toggle, `""` inside a quoted run is a literal,
  backslash escapes a following quote.

Two shipped bugs to avoid repeating:

- The dispatcher forwarded the *raw* command line, so the launcher's own path and
  switches reached the app. Forward `tail_override()`, never the raw line.
- `config.h`'s `ArgMode` default doubles as behaviour. `Custom` means "pass the tail";
  `None` means "pass nothing". Getting the default wrong silently broke passthrough.

## Launch flow

1. Resolve target (`target = auto` ⇒ `_<this exe>.exe` beside the launcher).
2. Resolve the desktop, unless `has_desktop` is false.
3. `switch = true` ⇒ switch first, then launch.
   `switch = false` (default) ⇒ launch, then find the window and move it, so the user
   is never dragged away.
4. Wait policy: `auto` waits for console targets (and when a console is attached),
   detaches GUI targets.
5. Return the target's exit code when waiting; `0` when detached.

Working directory defaults to the **target's** folder, matching how Windows resolves
the working directory from an executable path.

## Config schema

- Values expand `%EXE_DIR%`, `%EXE%`, `%TARGET%`, `%TARGET_DIR%`, `%CWD%`,
  `%APPDATA%`, `%LOCALAPPDATA%`, `%DESKTOP%`, `%VARS%`.
- `[apps.<glob>]` sections match this exe's name or the raw command line; **later
  matches win** (base `[launch]` first).
- `env.NAME=value` sets, `env.un.NAME=1` removes. Later definitions replace earlier
  ones, and an unset name is emitted as `NAME=` (the CRT's `getenv` then returns NULL).
- `desktop` is 1-based. `+n`/`-n` are relative, `new`, `current`, empty = new.
  Out-of-range grows by creating desktops when `create = true`.

## Deliberate choices

- **No same-bitness helper.** Placement runs on explorer's side, so one binary wraps
  either bitness. See [COM.md](COM.md).
- **Unverified slot tables are opt-in.** `experimental_layout` exists because a wrong
  slot faults; default behaviour is graceful refusal instead of a crash.
- **Errors are logged, not just shown.** A silent wrapper is undebuggable, so every run
  appends to `vdlaunch.log` and `--diag`/`--print-config` explain the resolved state.
- **`vdlaunch.log` is opened unconditionally at startup** so failures that happen while
  loading config are still recorded.
