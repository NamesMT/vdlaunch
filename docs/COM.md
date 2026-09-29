# The virtual-desktop COM engine

Unwritten here: `src/desktop.cpp` is the only file that talks to this API. Treat it
as the single source of truth for slot numbers.

## Why undocumented COM is required

- The documented `IVirtualDesktopManager::MoveWindowToDesktop` only moves windows
  **owned by the calling process** — useless for a launcher.
- The cross-process move is `IVirtualDesktopManagerInternal::MoveViewToDesktop`,
  which is undocumented and whose vtable **shifted in Windows 11 24H2**.

## Verified contract (build 26100 / 26200)

| Piece | Value |
| --- | --- |
| `CLSID_ImmersiveShell` | `{C2F03A33-21F5-47FA-B4BB-156362A2F239}` |
| service id | `{C5E0CDCA-7B6E-41B2-9FC4-D93975CC467B}` |
| `IVirtualDesktopManagerInternal` | `{53F5CA0B-158F-4124-900C-057158060B27}` |
| `IVirtualDesktop` | `{3F07F4BE-B107-441A-AF0F-39D82529072C}` |
| `IApplicationViewCollection` | `{1841C6D7-4F9D-42C0-AF41-8747538F10E5}` |
| `IVirtualDesktopManager` (public) | `{A5CD92FF-29BE-454C-8D04-D82879FB3F1B}` |

Slots are 0-based and **include `IUnknown`'s three entries**, so an interface's
first own method is slot 3.

| Method | Slot |
| --- | --- |
| `GetCount` | 3 |
| `MoveViewToDesktop` | 4 |
| `GetCurrentDesktop` | 6 |
| `GetDesktops` | 7 |
| `SwitchDesktop` | 9 |
| `CreateDesktopW` | 11 |
| `RemoveDesktop` | 13 |
| `IVirtualDesktop::GetID` | 4 |
| `IApplicationViewCollection::GetViewForHwnd` | 6 |
| public `IsWindowOnCurrentVirtualDesktop` / `GetWindowDesktopId` / `MoveWindowToDesktop` | 3 / 4 / 5 |

## Rules that cost real time

1. **Acquire the manager through `IServiceProvider::QueryService`.** `CoCreateInstance`
   on the manager CLSID returns `E_NOINTERFACE`. `QueryService(service=CLSID, iid=IID)` works.
2. **A wrong slot faults; it does not return an error.** Access violation, usually a
   silent process death. Never call a slot you have not verified.
3. **`HRESULT` cannot identify a slot.** Several wrong slots returned `S_OK` doing
   nothing. Confirm by *effect*: desktop count changed, current desktop changed.
4. **The vtable pointer is not evidence of the interface.** Manager and desktop objects
   reported the identical address inside `combase.dll` `.rdata` — a generic proxy.
5. **Bitness is irrelevant.** All of this runs on explorer's side, so a 64-bit build
   places a 32-bit app's window. Do not add a same-bitness helper.
6. **`GetVirtualDesktopId` on `IApplicationView` is not usable through this proxy.**
   Use the public manager's `GetWindowDesktopId` instead.

## Other Windows builds

The IID **and** the layout vary per revision, and are not cleanly correlated to the
build number:

| Build | IID |
| --- | --- |
| 26100+ | `53F5CA0B-…` |
| 22631 | `4970BA3D-…` |
| 22621 | `A3175F2D-…` |
| 21313 **and** 22449 | `B2F925B9-…` (same IID, **different layout**) |
| 20231 | `094AFE11-…` |
| 9000–19041 | `F31574D6-…` |

`pyvda` warns that the 26100 IID can also answer on 22631, so **probe candidate IIDs
newest-first rather than trusting a build number**. `init()` already does this and
records the match in `EngineInfo::iid`.

`src/desktop.cpp` carries a method table per revision (from pyvda) and picks one from
the IID that answered. That alone is not enough, because the IID is not a reliable
version signal — 21313 and 22449 share one but differ in layout.

So the mutating operations do not trust the table. They try the detected slot first,
then every other revision's value for that method, and accept one only by its effect:

| Operation | Accepted when |
| --- | --- |
| create desktop | the desktop count grew |
| switch desktop | the current desktop index changed |
| remove desktop | the desktop count dropped |
| move window | `MoveViewToDesktop` returned success — **stable at slot 4 on every revision** |

This is what makes a wrong guess survivable rather than fatal, and it means the older
rows do not need to be trusted blindly. Two mechanisms make it safe:

1. **Fault guard.** MinGW has no `__try`, and a wrong slot is an access violation, not
   an error. `src/guard.cpp` installs a vectored exception handler and `longjmp`s back,
   so the call returns `E_UNEXPECTED` and the probe continues instead of the process
   dying. Verified: a deliberately corrupted table is recovered from.
2. **Side-effect recovery.** A rejected candidate can still have switched the desktop
   (slot 9 *is* `SwitchDesktop` on 26100). `create_desktop` notes the current desktop
   first and switches back if a probe moved the view.

Only 26100/26200 has been checked by hand on real hardware. The older rows come from
published tables and the self-correction above; nobody has run this on a Windows 10
box yet, so treat that path as untested-but-guarded.

## Re-probing procedure

1. Pick or copy a program from `tools/` (see `tools/README.md`); build with
   `x86_64-w64-mingw32-g++ -std=c++17 -O2 -o probe.exe probe_x.cpp -lole32 -loleaut32 -luuid`.
2. Run it from a Windows-visible path — `cmd.exe` cannot use a UNC
   `\\wsl.localhost\...` working directory.
3. Probe one slot per process (`probe_isolate.cpp` pattern): `__try`/SEH is unavailable
   under MinGW, so a vectored exception handler plus a fresh process is how you survive
   a bad slot.
4. Verify each hit by effect, never by `HRESULT`.
5. Record the result in `src/desktop.cpp` **and** the table above, then delete the
   scratch probe or commit it under `tools/`.

## Useful probes

| Question | Program |
| --- | --- |
| Which slot creates a desktop? | `tools/probe_mk.cpp` |
| Which slot switches the desktop? | `tools/probe_final.cpp` |
| Which slot moves a foreign window? | `tools/probe_mvw.cpp` |
| Which slot removes a desktop? | `tools/probe_rm.cpp` |
| Why is this vtable a proxy? | `tools/probe_mod.cpp`, `tools/probe_vt.cpp` |
| Does a background launch stay put? | `tools/probe_bg.cpp` |
