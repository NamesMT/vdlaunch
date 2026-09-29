#pragma once
#include <windows.h>
#include <string>

namespace vd {

struct Cli {
  bool         print_config = false;
  bool         diag = false;
  bool         log = false;
  bool         version = false;
  bool         help = false;
  std::wstring ini;
  std::wstring target;   // internal, set by the auto dispatcher
  int          cleanup_keep = 0;  // >0: remove extra desktops and exit
  // Everything after our own switches: what the wrapped app should receive.
  std::wstring tail;
  bool         blocked = false;  // a switch was missing its value
};

// Parses the leading launcher switches and computes the passthrough tail.
Cli parse_cli(int argc, LPWSTR* argv, const std::wstring& raw_command_line);

}  // namespace vd
