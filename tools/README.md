# tools

Probe programs used to reverse-engineer the internal virtual-desktop COM API on a
live Windows host. Each one validates behaviour by effect, not by `HRESULT`.

Build any of them with:

```sh
x86_64-w64-mingw32-g++ -std=c++17 -O2 -o probe.exe probe_x.cpp -lole32 -loleaut32 -luuid
```

Run from a Windows-visible path (not a UNC `\\wsl.localhost` path) so `cmd.exe`
can execute it.

| Probe | Question it answers |
| --- | --- |
| `probe_com.cpp` | Can `ImmersiveShell` be activated, and does CLSID-as-IID ever work? |
| `probe_move.cpp` | Does the internal manager enumerate desktops, and which slots answer? |
| `probe_slots.cpp` | Which vtable slots are callable with which signature? |
| `probe_acq.cpp` | Which acquisition path yields a usable manager (vs. a proxy)? |
| `probe_isolate.cpp` | Per-slot probing with crash isolation, so faults don't hide results |
| `probe_avc.cpp` | Which slot is `IApplicationViewCollection::GetViewForHwnd`? |
| `probe_final.cpp` | Which slot actually switches desktops (`SwitchDesktop`)? |
| `probe_mvw.cpp` | Which slot actually moves a foreign window (`MoveViewToDesktop`)? |
| `probe_mk.cpp` | Which slot creates a desktop (`CreateDesktopW`)? |
| `probe_rm.cpp` | Which slot removes a desktop (`RemoveDesktop`)? |
| `probe_bg.cpp` | Does a background launch stay on the user's desktop? |
| `probe_mod.cpp`, `probe_vt.cpp` | Why is the reported vtable a generic proxy? |
| `probe_state.cpp` | Current desktop list/state snapshot |

Results are recorded in `src/desktop.cpp`; the verified contract, the gotchas and the
re-probing procedure are documented in [`docs/COM.md`](../docs/COM.md).

Note: probe sources are kept as evidence of how each slot was established. They are
scratch programs — they are not built by `make` and are not part of the launcher.
