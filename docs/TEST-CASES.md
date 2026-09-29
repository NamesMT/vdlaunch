# Test suite map

`test/run.sh` drives the real Windows host through `cmd.exe` interop. It needs a live
desktop session; the COM sections skip cleanly when no manager is available.

Mocks: `test/mock.c` is built both ways into `.mocks/` and records `argv`, `cwd`, the
environment and, in GUI mode, which desktop its window is on.

## Sections

| ID | Section | Covers |
| --- | --- | --- |
| 1 | zero-config rename | `_name.exe` resolution with no ini, argv[0] is the real target |
| 2 | awkward arguments | spaces, `=`, `/slash`, embedded quote, UTF-8, inner spaces |
| 3 | app rules, env, cwd | `[apps.*]` matching, `env.`/`env.un.`, `%EXE_DIR%` |
| 4 | cwd modes | `cwd = target` vs `cwd = inherit` |
| 5 | args modes | `args = none` / a literal replacement |
| 6 | cross-bitness | x64 launcher runs both 32- and 64-bit targets |
| 7 | error handling | missing target exits non-zero; self-target refused |
| 8 | desktop placement | the real COM behaviour; skips without a manager |
| 9 | vdlaunchCreator | dry run, wrap, passthrough after wrapping, double-wrap refusal, key omission, bitness pick, ini backup, missing app |

Section 8 detail:

| ID | Asserts |
| --- | --- |
| 8a | background launch onto `current` does not switch the visible desktop |
| 8b | `desktop = new` creates a desktop, user stays put, window moves away |
| 8b2 | a **32-bit** GUI target's window also moves, launcher stays in background |
| 8c | a missing index is auto-created (`create = true`) |
| 8d | `create = false` adds nothing and the app still launches |
| 8e | `switch = true` really moves the visible desktop, then restores it |
| 8f | the suite restored the original desktop count |

## Adding a case

1. Add a `fresh sN` scenario and copy the launcher plus a mock in.
2. Write the ini and use `write_runner` (never hand-roll a `cmd` one-liner).
3. `run_scenario`, then assert with `contains`/`check`.
4. If it creates desktops, extend the cleanup so the host is left as found.
5. Keep the assertion ID naming (`8b2.1`) — failures are read by ID.

## Reading a failure

- `no such file or directory` on the captured output ⇒ the target never wrote it; check
  the launcher's `vdlaunch.log` in that scenario directory first.
- A scenario that appears to hang ⇒ a modal dialog; set `quiet = true`.
- A failing assertion that reproduces in `--print-config` but not in code ⇒ the ini
  beside the **exe** is what counts, not the current directory.

## Keep it honest

Every assertion must fail if the behaviour regresses. A test that cannot distinguish a
correct run from a broken one is worse than no test — the older-build path, for example,
is deliberately not covered because it cannot be simulated on this host.
