# vdlaunch

**Launch any Windows program on a chosen virtual desktop — by just renaming it.**

No shortcuts to edit, no flags to remember, no wrapper scripts. Rename your app to
`_yourApp.exe`, drop `vdlaunch.exe` in its place, and every launch lands where
`vdlaunch.ini` says. Arguments pass through untouched.

```
yourApp.exe          ->  _yourApp.exe       (the real app)
vdlaunch.exe         ->  yourApp.exe        (the wrapper)
vdlaunch.ini                                (which desktop to use)
```

```
$ vdlaunch-demo.exe --diag     # before: you are working on Desktop 1
desktop count : 1
current       : 1

$ myApp.exe                    # the wrapped app, launched normally
                               # ... you never leave Desktop 1 ...

$ vdlaunch-demo.exe --diag     # after
desktop count : 2              # Desktop 2 appeared on its own
current       : 1              # and you are still on Desktop 1
```

The wrapped app's window reports `onCurrentDesktop=0` with a desktop id matching
Desktop 2 — it is running over there, not here.

## Why

Virtual-desktop tools usually make you drive them:

```bat
vdesk on:2 run:"C:\Program Files\App\app.exe" --some-flag
```

That breaks the moment something else launches your app — a shortcut, a file
association, a `Start-Process`, an updater. vdlaunch inverts it: the app *is* the
launcher, so every existing entry point keeps working, and the desktop choice lives
in a config file next to it.

## Quick start

1. Grab `vdlaunch64.exe` (or `vdlaunch32.exe` for 32-bit apps) from
   [Releases](../../releases) — or use `vdlaunch.exe`, which picks the right one.
2. In your app's folder, rename `app.exe` → `_app.exe`.
3. Copy `vdlaunch.exe` in and rename it to `app.exe`.
4. Optional: copy `vdlaunch.ini.example` next to it as `vdlaunch.ini` and set `desktop`.

That's it. No config is required.

## Configuration

`vdlaunch.ini` sits beside the launcher. Every key is optional.

```ini
[launch]
target  = auto        ; auto = _<this exe>.exe, or an explicit path
desktop = 2           ; 1-based, matching "Desktop 2" in Task View
switch  = false       ; false = launch silently in the background
create  = true        ; create the desktop if it does not exist
wait    = auto        ; auto = wait for console apps, detach GUI apps
cwd     = target      ; target | inherit | <a path>
```

<details>
<summary><b>Values for <code>desktop</code></b></summary>

| Value | Meaning |
| --- | --- |
| `2` | Desktop 2, created first if needed (`create = true`) |
| `+1` / `-1` | Relative to the desktop you are on now |
| `current` | Leave it on the current desktop |
| `new` | Always a brand-new desktop |
| *(empty)* | Same as `new` |

Indexes are **1-based**, matching how Windows labels desktops in Task View.
An out-of-range index is created automatically when `create = true`; with
`create = false` it is refused and the app still launches on the current desktop.
</details>

<details>
<summary><b>Background vs. switch mode</b></summary>

`switch = false` (default) launches the app **without dragging you away**: it is
started normally, its window is then moved onto the target desktop, and your view
stays where it was. If the window never appears (`window_timeout`, default 15 s)
the app simply stays on the current desktop.

`switch = true` switches the visible desktop to the target first, then launches.
This is more reliable for single-instance apps and browsers that hand the window
to an already-running process.
</details>

<details>
<summary><b>Per-app rules</b></summary>

Sections named `[apps.<glob>]` are matched against this exe's file name and against
the raw command line; later matches win. Handy for one launcher copy serving
several apps, or varying the desktop per invocation.

```ini
[apps.chrome.exe]
target  = C:\Program Files\Google\Chrome\Application\chrome.exe
desktop = 3

[apps.*--new-window*]
desktop = +1
```
</details>

<details>
<summary><b>Environment and arguments</b></summary>

```ini
env.MY_VAR     = value    ; set/override
env.un.OLD_VAR = 1        ; remove
```

`%EXE_DIR%`, `%EXE%`, `%TARGET%`, `%TARGET_DIR%`, `%CWD%`, `%APPDATA%`,
`%LOCALAPPDATA%`, `%DESKTOP%` and ordinary `%VARS%` expand anywhere a value is used.

```ini
args = all      ; pass vdlaunch's own command line too
args = none     ; pass nothing
args = --flag   ; pass exactly this
```
</details>

<details>
<summary><b>Command-line switches</b></summary>

Only leading switches are ours; everything else belongs to the app.

| Switch | Effect |
| --- | --- |
| `--print-config` | Print the resolved configuration and exit |
| `--diag` | Report COM / virtual-desktop diagnostics |
| `--ini <path>` | Use another ini file |
| `--cleanup-desktops [n]` | Remove extra desktops down to `n` (default 1) |
| `--log` | Print to the attaching console |
| `--version`, `--help` | |

Every run appends to `vdlaunch.log` next to the launcher, which is the only way to
see what a silent wrapper did.
</details>

## Builds

| File | Use |
| --- | --- |
| `vdlaunch64.exe` | x64 targets — rename over `yourApp.exe` |
| `vdlaunch32.exe` | 32-bit targets — rename over `yourApp.exe` |
| `vdlaunch.exe` | Auto: reads the target's PE header and hands a 32-bit app to `vdlaunch32.exe`. Keep both helpers beside it. |

Build from WSL/Linux with mingw-w64:

```sh
sudo pacman -S mingw-w64-gcc      # or: apt install g++-mingw-w64-x86-64 g++-mingw-w64-i686
make                              # dist/vdlaunch64.exe, vdlaunch32.exe, vdlaunch.exe
make console                      # console-subsystem twins for scripting/tests
make test                         # full scenario suite (needs a Windows desktop session)
```

## How it works

<details>
<summary><b>Virtual-desktop engine (the interesting part)</b></summary>

Windows has a documented `IVirtualDesktopManager`, but it can only move **its own**
process's windows. Cross-process placement needs the internal manager, which is
undocumented and whose COM vtable **shifted in Windows 11 24H2** — so the IIDs and
slot numbers circulating since 2018 are wrong on current builds.

This project pins them down empirically for build 26100/26200. Reached through
`ImmersiveShell`'s `IServiceProvider` (its CLSID alone returns `E_NOINTERFACE`):

| Method | Vtable slot |
| --- | --- |
| `GetCount` / `MoveViewToDesktop` | 3 / 4 |
| `GetCurrentDesktop` / `GetDesktops` | 6 / 7 |
| `SwitchDesktop` / `CreateDesktopW` | 9 / 11 |
| `RemoveDesktop` | 13 |
| `IVirtualDesktop::GetID` | 4 |
| `IApplicationViewCollection::GetViewForHwnd` | 6 |

Slots count `IUnknown`'s three entries. `tools/` keeps the probe programs used to
discover them, each validated by observing the real effect (a desktop actually
changes, a window actually moves) rather than trusting an `HRESULT`.
</details>

## Limitations

- Windows 10/11 only. Without a virtual-desktop manager the app still launches, just on the current desktop.
- Background mode needs the app to open a window within `window_timeout`; single-instance apps are better served by `switch = true`.
- The auto build needs `vdlaunch32.exe` next to it when wrapping a 32-bit app.

## Credits

Built on the shoulders of [eksime/VDesk](https://github.com/eksime/VDesk) — the
original `vdesk run:` tool whose config-driven, drop-in approach this reimagines,
and the reason this project is GPL-3.0. Desktop IIDs and vtable layout were
cross-checked against [Ciantic/VirtualDesktopAccessor](https://github.com/Ciantic/VirtualDesktopAccessor)
and [mirober/pyvda](https://github.com/mirober/pyvda).

GPL-3.0-or-later · see [LICENSE](LICENSE).
