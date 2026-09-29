#pragma once
#include <string>
#include <vector>

namespace vd {

enum class ArgMode    { Custom, None, All };
enum class WaitMode   { Auto, Always, Never };
enum class CwdMode    { Target, Inherit, Custom };

struct Config {
  bool loaded = false;          // an ini was found and read
  bool auto_target = true;      // target=auto -> _<thisself>.exe
  std::wstring target;          // resolved target executable
  std::string  target_raw;

  ArgMode  args = ArgMode::Custom;  // pass the tail through by default
  std::string args_custom;

  bool has_desktop = false;     // false -> do not touch virtual desktops
  std::string desktop;          // "3", "+1", "new", "current"
  bool switch_to  = false;      // false = silent background launch (default)
  bool create     = true;
  WaitMode wait   = WaitMode::Auto;
  CwdMode  cwd    = CwdMode::Target;
  std::wstring cwd_custom;
  int  window_timeout_ms = 15000;
  bool log = true;
  bool quiet = false;

  std::vector<std::pair<std::wstring, std::wstring>> env_set;    // added/overridden
  std::vector<std::wstring>                          env_unset;  // removed

  std::string ini_path;         // absolute path consulted
  std::string matched_rule;     // name of the [apps.*] section applied, if any
};

// `command_line` is the raw GetCommandLineW() of this process.
void set_ini_override(const std::wstring& path);
const std::wstring& ini_path_override();

Config config_load(const std::wstring& command_line);

// Passthrough tail (arguments for the wrapped app) after launcher switches are
// stripped; also used for [apps.*] rule matching.
void set_tail_override(const std::wstring& tail);
const std::wstring& tail_override();

// Expands %EXE_DIR%, %EXE%, %TARGET_DIR%, %TARGET%, %CWD%, %APPDATA%,
// %LOCALAPPDATA%, %DESKTOP%, %VAR%.
std::wstring expand_env(const std::wstring& in, const Config& cfg);

std::string config_summary(const Config& cfg);

}  // namespace vd
