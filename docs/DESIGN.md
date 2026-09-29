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
| `src/creator.cpp` | `vdlaunchCreator.exe`: prompts, renames in place, writes the ini |
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

## Config

Schema and key semantics: [CONFIG.md](CONFIG.md). Design points worth knowing here:

- `[apps.<glob>]` sections match this exe's name or the raw command line; later
  matches win, so `[launch]` is read first.
- An unset name is emitted as `NAME=` rather than dropped, because that is what makes
  the CRT's `getenv` return NULL for it.
- Later `env.`/`env.un.` definitions replace earlier ones.

## The creator (`vdlaunchCreator.exe`)

One binary for either target bitness: `src/creator.rc` embeds both launcher builds as
RCDATA (ids 1001/1002) and the creator picks by the target's PE machine field.

It mutates a user's app folder, so the order of operations is deliberate:

1. Validate everything (target exists, name is sane) before touching anything.
2. Refuse if the target already carries the `vdlaunch-wrapper` marker, or if
   `_name.exe` exists (unless `--force`) — a second wrap would overwrite the backup.
3. **Extract the launcher to a staging file first**, so an extraction failure leaves
   the folder untouched.
4. Rename `name.exe` → `_name.exe`, then move staging into place.
5. Write the ini last; any failure rolls the rename back.

`looks_like_launcher` scans for the marker with `std::string::find`, **not `strstr`** —
a PE is full of NUL bytes and `strstr` stops at the first one.

## Deliberate choices

- **No same-bitness helper.** Placement runs on explorer's side, so one binary wraps
  either bitness. See [COM.md](COM.md).
- **Mutating slots are accepted by effect, not by table.** A wrong slot faults, so the
  guard turns it into `E_UNEXPECTED` and the next candidate is tried.
- **Errors are logged, not just shown.** A silent wrapper is undebuggable, so every run
  appends to `vdlaunch.log` and `--diag`/`--print-config` explain the resolved state.
- **`vdlaunch.log` is opened unconditionally at startup** so failures that happen while
  loading config are still recorded.
