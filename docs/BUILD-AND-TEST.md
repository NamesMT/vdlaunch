# Building and testing

## Build

Cross-compiles from Linux/WSL with mingw-w64. No Windows toolchain needed.

```sh
sudo pacman -S mingw-w64-gcc          # or: apt install g++-mingw-w64-x86-64 g++-mingw-w64-i686
make                                  # dist/vdlaunch64.exe, dist/vdlaunch32.exe
make console                          # dist-test/*c.exe — console subsystem twins
make test                             # builds everything, then runs test/run.sh
make clean
```

Artifacts:

| Target | Why |
| --- | --- |
| `-mwindows` | Real launcher: no console flash for GUI apps |
| `-mconsole` twins | **Only** so the harness can capture stdout/stderr |
| `vdlaunchCreator.exe` | `-mconsole` on purpose: it is a prompt-driven tool |

`src/creator.rc` embeds `dist/vdlaunch64.exe` and `dist/vdlaunch32.exe` as RCDATA, so the
creator **must be built after both launchers** — the Makefile encodes that dependency.

The twins are the same objects, linked differently — never ship them.

## Environment facts that bite

- `pacman` needs root and `sudo` is passwordless here (`sudo -n pacman …`).
- **`cmd.exe` cannot use a WSL path as its working directory.** It prints
  `UNC paths are not supported` and silently runs from `C:\Windows`. Always
  `cd /d C:\…` inside the command, and keep test files somewhere under `/mnt/c/`.
- **The file sandbox grants `danger-full-access`** in this project, so no escalation
  is needed to write under `/mnt/c/`.
- `/tmp` is not stable across sessions — the harness keeps its mock binaries in
  `.mocks/` (gitignored) for that reason.

## Talking to the Windows host

Interop works: `cmd.exe /c "…"` and `powershell.exe -File …` both run. Prefer the
**console twins** for anything whose output you need.

- A **GUI-subsystem exe does not block `cmd.exe`**, so redirected output races and
  looks empty. Use `start /wait "" prog.exe`, or a console twin.
- Console apps inherited from `cmd` see an attached console, and `wait = auto`
  therefore waits — fine, but it means a test `.bat` blocks until the child exits.

## Writing test harness code

- **Build a `.bat` wrapper and run that.** A one-liner with `set "VAR=value"&& prog`
  leaks the trailing space into the value and creates stray files; the harness's
  `write_runner` exists to avoid this.
- **Do not put more than one `&`-chained command** in a `cmd /c` line if you need the
  output file — the file handle closes before a GUI process writes.
- Convert paths with the harness's `winpath` (a literal backslash in the format
  string); `printf '%s\\%s'` through `$(…)` collapses to a single `\` and silently
  writes a **file** instead of a directory path.
- A Windows **empty argument cannot be expressed** through `cmd`, so don't assert one.
- `cmd` collapses an argument that is only trailing spaces; assert an inner space
  (`--name=a b`) instead.

## Regression testing the desktop engine without breaking the host

- Snapshot `--diag` before and after; assert the count and current index are restored.
- Remove scratch desktops with `--cleanup-desktops <n>` at the end of the suite.
- A **failed desktop resolve raises a modal dialog** unless `quiet = true`; in an
  automated run that looks like a hang. Set `quiet = true` in scenario inis.
- To exercise the older-build path, temporarily force `g_info.build` and/or drop the
  newest IID from `kIids[]`, then **restore `src/desktop.cpp`** (`cp` a backup) before
  committing. Never leave a simulation in the tree.
