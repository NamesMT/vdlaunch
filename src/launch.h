#pragma once
#include "config.h"
#include <windows.h>
#include <string>

namespace vd {

struct LaunchPlan {
  std::wstring target;
  std::wstring cmdline;
  std::wstring workdir;
  bool         use_workdir = false;
};

bool build_plan(const Config& cfg, const std::wstring& raw_command_line, LaunchPlan* plan, std::string* error);

// Runs the whole launch. Returns the process exit code (or 0 when detached).
int run(const Config& cfg, const std::wstring& raw_command_line);

}  // namespace vd
