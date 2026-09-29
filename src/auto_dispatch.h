#pragma once
#include "config.h"
#include <windows.h>
#include <string>

namespace vd {

// Empty unless the auto dispatcher forwarded --ini.

// Overrides the vdlaunch.ini path (used by the auto dispatcher so the 32-bit
// launcher reads the same config that sits beside the renamed executable).


// Process image machine of a PE, or 0 when unreadable.
const std::wstring& ini_path_override();

unsigned short pe_machine(const std::wstring& path);

// True when this build should hand a 32-bit target to vdlaunch32.exe.
bool auto_dispatch_needed(const std::wstring& target);
int  auto_dispatch(const std::wstring& target, const std::wstring& raw_command_line);

}  // namespace vd
