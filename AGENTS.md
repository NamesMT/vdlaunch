# AGENTS.md

Rules for agents working in this repo.

- Build with `make` (mingw-w64 cross-compile from Linux/WSL). `make console` adds stdout-capable twins for scripting; `make test` runs the suite.
- The Makefile tracks `src/*.h`, but enum/struct changes still warrant `make clean` when a build looks stale.
- Never trust an `HRESULT` from the internal COM manager: a wrong vtable slot faults or silently no-ops. Verify by effect (desktop count changed, window moved).
- The launcher talks to explorer, not to the target, so it places windows of any bitness. Keep it that way: no same-bitness helper.
- Vtable slots in `src/desktop.cpp` are validated for Windows 11 build 26100/26200 only. Re-probe with `tools/probe_*.cpp` on any other build before editing them.
- `test/run.sh` drives the real Windows host through cmd.exe interop and needs a live desktop session; it restores the desktop count when it finishes.
- Keep passthrough byte-exact: never re-quote or reorder the wrapped app's arguments.
MDEOF
cat > tools/README.md <<'EOF'
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
| `probe_state.cpp` | Current desktop list/state snapshot |

Results are recorded in `src/desktop.cpp` and the README's "How it works" section.
