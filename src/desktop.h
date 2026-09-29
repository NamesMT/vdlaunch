#pragma once
#include <windows.h>
#include <string>
#include <vector>

namespace vd {

struct DesktopInfo {
  std::wstring id;
  int          index = 0;  // 1-based
};

// True when this OS exposes the internal virtual-desktop manager we use.
bool  available();
// Initializes COM and resolves the manager. Safe to call repeatedly.
bool  init();
void  shutdown();

int                 count();
std::vector<DesktopInfo> list();
// 1-based index of the desktop the shell currently shows, or -1.
int                 current_index();
// Creates a desktop without switching to it. Returns its 1-based index, or -1.
int                 create_desktop();
// Removes a desktop, moving its windows to `fallback`. Both are 1-based.
bool                remove_desktop(int index, int fallback);
// Switches the visible desktop. `index` is 1-based.
bool                switch_desktop(int index);
// Moves an existing window onto the given 1-based desktop without switching.
bool                move_window(HWND hwnd, int index);
// The desktop a window currently sits on, 1-based, or -1 when unknown.
int                 window_desktop(HWND hwnd);

struct ResolvedDesktop {
  int  index = 0;      // resulting 1-based index
  bool created = false;
  bool ok = false;
  std::string error;
};

// spec: "3", "+2", "-1", "new", "current", or a desktop name.
// Empty spec resolves to a new desktop.
ResolvedDesktop resolve(const std::string& spec, bool allow_create);

}  // namespace vd
