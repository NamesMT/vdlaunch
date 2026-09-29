# Config reference

## Creating a config (vdlaunchCreator.exe)

Put the creator in the app's folder and run it. It prompts for the program name, desktop
and the optional keys, then renames `name.exe` → `_name.exe`, writes the matching-bitness
launcher as `name.exe`, and writes `vdlaunch.ini`. Blank answers omit the key, so the
launcher default applies.

```sh
vdlaunchCreator.exe --name myapp --desktop 2 --switch false --create true -y
vdlaunchCreator.exe --name myapp --desktop +1 --dry-run    # preview only
```

| Switch | Effect |
| --- | --- |
| `--name <name>` | program name without `.exe` (prompted if absent) |
| `--desktop <spec>` | `2`, `+1`, `-1`, `current`, `new` |
| `--switch <bool>` / `--create <bool>` | as in the ini; omit to leave the key out |
| `--wait <mode>` | `auto`, `always`, `never` |
| `--dir <path>` | folder to work in (default: this exe's folder) |
| `--dry-run` | show the plan, change nothing |
| `--force` | overwrite an existing `_name.exe` |
| `-y`, `--yes` | skip the confirmation prompt |

Refuses to wrap an app that is already a launcher, and keeps a previous `vdlaunch.ini`
as `vdlaunch.ini.bak`. If any write fails it restores the original name.


`vdlaunch.ini` sits beside the launcher. Every key is optional; the shortest
working file is in [`../vdlaunch.ini.example`](../vdlaunch.ini.example).

Keys written before any `[section]` header count as `[launch]`, so a one-line
`desktop = 2` is enough on its own.

## `[launch]`

| Key | Values | Default |
| --- | --- | --- |
| `target` | `auto`, a path, or a file name | `auto` = `_<this exe>.exe` beside the launcher |
| `desktop` | `2`, `+1`, `-1`, `current`, `new`, blank | `new` |
| `desktop_off` | `true` to leave virtual desktops alone | off |
| `switch` | `true` switches your visible desktop first | `false` (background) |
| `create` | create the desktop when the index is out of range | `true` |
| `wait` | `auto`, `always`, `never` | `auto` |
| `cwd` | `target`, `inherit`, or a path | `target` |
| `window_timeout` | milliseconds to wait for the window | `15000` |
| `args` | `all`, `none`, or a literal string | pass the caller's arguments |
| `log` | also write to the attaching console | off |
| `quiet` | suppress dialogs | off |
| `env.NAME` | sets `NAME` | |
| `env.un.NAME` | removes `NAME` (any value but `0`) | |

`desktop` is 1-based, matching Task View. `+n`/`-n` are relative to the desktop you
are on. Out-of-range indexes grow by creating desktops when `create = true`.

## `[apps.<glob>]`

Matched against this exe's file name and against the raw command line; **later
matches win**, so put `[launch]` first.

```ini
[apps.chrome.exe]
target  = C:\Program Files\Google\Chrome\Application\chrome.exe
desktop = 3

[apps.*--new-window*]
desktop = +1
```

## Values

`%EXE_DIR%`, `%EXE%`, `%TARGET%`, `%TARGET_DIR%`, `%CWD%`, `%APPDATA%`,
`%LOCALAPPDATA%`, `%DESKTOP%` and ordinary `%VARS%` expand in any value.

## Launcher switches

Leading position only; everything else goes to the app.

| Switch | Effect |
| --- | --- |
| `--print-config` | print the resolved configuration and exit |
| `--diag` | print build, COM interface and desktop state |
| `--ini <path>` | use another ini file |
| `--cleanup-desktops [n]` | remove extra desktops down to `n` (default 1) |
| `--log` | print to the attaching console |
| `--version`, `--help` | |
