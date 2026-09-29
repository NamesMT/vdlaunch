# vdlaunch

**Start any Windows app on a chosen virtual desktop — by renaming it.**

Rename your app to `_app.exe`, put `vdlaunch64.exe` in its place, and it launches on
the desktop you configure. Arguments pass through untouched, so shortcuts, file
associations and updaters keep working.

```
app.exe          ->  _app.exe          the real app
vdlaunch64.exe   ->  app.exe           the wrapper
vdlaunch.ini                           your desktop choice
```

## Set up (2 minutes)

1. Download `vdlaunch64.exe` and `vdlaunch.ini.example` from [Releases](../../releases).
2. In your app's folder, rename `app.exe` → `_app.exe`.
3. Copy `vdlaunch64.exe` in and rename it to `app.exe`.
4. Copy `vdlaunch.ini.example` in as `vdlaunch.ini`. It already says "Desktop 2":

```ini
[launch]
target  = auto   ; auto = _<this exe>.exe in the same folder
desktop = 2      ; 1-based, so this is "Desktop 2" in Task View
switch  = false  ; launch in the background without leaving your desktop
create  = true   ; create the desktop if it does not exist
```

Launch the app the way you always do. It opens on Desktop 2; you stay where you are.

No ini at all also works — it falls back to `_app.exe` on the current desktop.

## Check it

```
app.exe --diag          build, COM interface, desktop list — send this when reporting a problem
app.exe --print-config  what vdlaunch resolved, before launching anything
```

Every run appends to `vdlaunch.log` beside the launcher.

## Options

Everything else — desktop selectors, background vs. switch, per-app rules, environment
overrides, cwd and argument modes — is in **[docs/CONFIG.md](docs/CONFIG.md)**.

## Which file

| File | Use |
| --- | --- |
| `vdlaunch64.exe` | Use this unless you have a reason not to |
| `vdlaunch32.exe` | Same features, x86 host |

Either build wraps 32-bit and 64-bit targets alike, and neither needs a helper file.

## Windows support

| Build | Behaviour |
| --- | --- |
| 26100+ (11 24H2 / 25H2) | Full support |
| Older | Launches onto an existing desktop; refuses to create or switch, and logs why |

Nothing crashes on older builds. See [docs/COM.md](docs/COM.md) for the interface
details and how to add a build.

## Development

```sh
sudo pacman -S mingw-w64-gcc     # or: apt install g++-mingw-w64-x86-64 g++-mingw-w64-i686
make                             # dist/vdlaunch64.exe, vdlaunch32.exe
make console                     # stdout-capable twins for scripting
make test                        # scenario suite (needs a Windows desktop session)
```

- [AGENTS.md](AGENTS.md) — guardrails
- [docs/](docs/README.md) — COM contract, build/test host gotchas, test map, design

## Credits

Built on [eksime/VDesk](https://github.com/eksime/VDesk), the original `vdesk run:`
tool whose config-driven, drop-in idea this reworks — and the reason this is GPL-3.0.
Slot data cross-checked against [Ciantic/VirtualDesktopAccessor](https://github.com/Ciantic/VirtualDesktopAccessor)
and [mirober/pyvda](https://github.com/mirober/pyvda).

GPL-3.0-or-later · see [LICENSE](LICENSE).
