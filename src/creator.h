#pragma once
#include <string>

namespace vd {

struct CreatorOptions {
  std::wstring dir;          // folder to operate in; defaults to this exe's folder
  std::wstring name;         // program name, without .exe
  std::string  desktop;      // blank = leave the key out
  bool         have_switch = false;
  bool         switch_to   = false;
  bool         have_create = false;
  bool         create      = false;
  std::string  wait;         // blank = leave the key out
  bool         assume_yes  = false;
  bool         force       = false;
  bool         dry_run     = false;
};

int creator_run(CreatorOptions& opt);
void creator_usage();
void attach_parent_console_creator();

}  // namespace vd
