#include "config.h"
#include "ini.h"
#include "util.h"
#include <windows.h>
#include <shlobj.h>
#include <algorithm>

namespace vd {

static std::wstring g_ini_override;
static std::wstring g_tail_override;
static bool         g_tail_set = false;

void set_ini_override(const std::wstring& path) { g_ini_override = path; }

const std::wstring& ini_path_override() { return g_ini_override; }
void set_tail_override(const std::wstring& tail) { g_tail_override = tail; g_tail_set = true; }
const std::wstring& tail_override() { return g_tail_override; }

static std::string command_line_tail(const std::wstring& command_line) {
  // Skip argv[0]; it may be quoted.
  size_t i = 0;
  while (i < command_line.size() && (command_line[i] == L' ' || command_line[i] == L'\t')) i++;
  if (i < command_line.size() && command_line[i] == L'"') {
    i++;
    while (i < command_line.size() && command_line[i] != L'"') i++;
    if (i < command_line.size()) i++;
  } else {
    while (i < command_line.size() && command_line[i] != L' ' && command_line[i] != L'\t') i++;
  }
  while (i < command_line.size() && (command_line[i] == L' ' || command_line[i] == L'\t')) i++;
  return utf8(command_line.substr(i));
}

std::wstring expand_env(const std::wstring& in, const Config& cfg) {
  std::wstring out;
  for (size_t i = 0; i < in.size(); i++) {
    if (in[i] != L'%') { out += in[i]; continue; }
    size_t end = in.find(L'%', i + 1);
    if (end == std::wstring::npos) { out += in[i]; continue; }
    std::wstring name = in.substr(i + 1, end - i - 1);
    std::wstring repl;

    if (iequalsw(name, L"EXE_DIR"))       repl = exe_dir();
    else if (iequalsw(name, L"EXE"))      repl = exe_path();
    else if (iequalsw(name, L"TARGET"))   repl = cfg.target;
    else if (iequalsw(name, L"TARGET_DIR")) repl = path_dir(cfg.target);
    else if (iequalsw(name, L"CWD")) {
      wchar_t buf[MAX_PATH * 2] = {};
      GetCurrentDirectoryW(MAX_PATH * 2, buf);
      repl = buf;
    } else if (iequalsw(name, L"APPDATA") || iequalsw(name, L"LOCALAPPDATA") || iequalsw(name, L"TEMP")) {
      wchar_t buf[MAX_PATH * 2] = {};
      DWORD n = GetEnvironmentVariableW(name.c_str(), buf, MAX_PATH * 2);
      if (n > 0 && n < MAX_PATH * 2) repl = buf;
    } else if (iequalsw(name, L"DESKTOP")) {
      wchar_t buf[MAX_PATH] = {};
      if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_DESKTOPDIRECTORY, nullptr, 0, buf))) repl = buf;
    } else {
      wchar_t buf[MAX_PATH * 4] = {};
      DWORD n = GetEnvironmentVariableW(name.c_str(), buf, MAX_PATH * 4);
      if (n > 0 && n < MAX_PATH * 4) repl = buf;
    }

    if (repl.empty()) out += in.substr(i, end - i + 1);
    else out += repl;
    i = end;
  }
  return out;
}

static bool truthy(const std::string& v, bool dflt) {
  std::string s = lower(trim(v));
  if (s.empty()) return dflt;
  if (s == "1" || s == "true" || s == "yes" || s == "on") return true;
  if (s == "0" || s == "false" || s == "no" || s == "off") return false;
  return dflt;
}

static void apply_section(Config& cfg, const IniSection& s) {
  for (auto& e : s.entries) {
    std::string k = lower(e.key);
    const std::string& v = e.value;

    if (k == "target") {
      if (iequals(trim(v), "auto")) {
        cfg.auto_target = true;
      } else {
        cfg.auto_target = false;
        cfg.target_raw = v;
      }
    } else if (k == "desktop" || k == "desktop_index") {
      cfg.has_desktop = true;
      cfg.desktop = v;
    } else if (k == "desktop_off") {
      cfg.has_desktop = false;
    } else if (k == "switch") {
      cfg.switch_to = truthy(v, cfg.switch_to);
    } else if (k == "create") {
      cfg.create = truthy(v, cfg.create);
    } else if (k == "wait") {
      std::string t = lower(trim(v));
      if (t == "true" || t == "1" || t == "always") cfg.wait = WaitMode::Always;
      else if (t == "false" || t == "0" || t == "never") cfg.wait = WaitMode::Never;
      else cfg.wait = WaitMode::Auto;
    } else if (k == "args") {
      std::string t = lower(trim(v));
      if (t == "all") cfg.args = ArgMode::All;
      else if (t == "none") { cfg.args = ArgMode::None; cfg.args_custom.clear(); }
      else if (!t.empty()) { cfg.args = ArgMode::Custom; cfg.args_custom = v; }
    } else if (k == "cwd") {
      std::string t = lower(trim(v));
      if (t == "target") cfg.cwd = CwdMode::Target;
      else if (t == "inherit") cfg.cwd = CwdMode::Inherit;
      else if (!t.empty()) { cfg.cwd = CwdMode::Custom; cfg.cwd_custom = wide(v); }
    } else if (k == "window_timeout" || k == "window_timeout_ms") {
      cfg.window_timeout_ms = std::max(0, atoi(v.c_str()));
    } else if (k == "experimental_layout") {
      cfg.experimental_layout = truthy(v, cfg.experimental_layout);
    } else if (k == "log") {
      cfg.log = truthy(v, cfg.log);
    } else if (k == "quiet") {
      cfg.quiet = truthy(v, cfg.quiet);
    } else if (e.key.compare(0, 4, "env.") == 0) {
      // env.NAME=value   -> set
      // env.un.NAME=1    -> remove (any other value is a no-op)
      std::string rest = e.key.substr(4);
      if (rest.compare(0, 3, "un.") == 0)
        cfg.env_unset.push_back(wide(rest.substr(3)));
      else
        cfg.env_set.emplace_back(wide(rest), wide(v));
    }
  }
}

Config config_load(const std::wstring& command_line) {
  Config cfg;
  std::wstring path = g_ini_override.empty() ? path_join(exe_dir(), L"vdlaunch.ini") : g_ini_override;
  cfg.ini_path = utf8(path);

  IniFile ini = ini_parse_file(cfg.ini_path);
  if (!ini.loaded) {
    logf("config: no ini at %s; using defaults", cfg.ini_path.c_str());
  } else {
    cfg.loaded = true;
    if (auto* base = ini.section("launch")) apply_section(cfg, *base);

    // Later matching [apps.*] sections win.
    std::string tail = g_tail_set ? utf8(g_tail_override) : command_line_tail(command_line);
    for (auto* s : ini.sections_like("apps.*")) {
      std::wstring pattern = wide(s->name.substr(5));
      std::wstring self = exe_name();
      if (!glob_match(pattern, self) && !glob_match(pattern, wide(tail))) continue;
      apply_section(cfg, *s);
      cfg.matched_rule = s->name;
      logf("config: applied section [%s]", s->name.c_str());
    }
  }

  if (cfg.auto_target) {
    std::wstring self = exe_name();
    std::wstring base = self;
    size_t dot = base.find_last_of(L'.');
    if (dot != std::wstring::npos) base = base.substr(0, dot);
    cfg.target_raw = utf8(L"_" + base + L".exe");
  }

  if (!cfg.target_raw.empty()) {
    if (cfg.target_raw.find('%') != std::string::npos) cfg.target_raw = utf8(expand_env(wide(cfg.target_raw), cfg));
    cfg.target = path_abs(wide(cfg.target_raw));
  }
  if (cfg.cwd == CwdMode::Custom) cfg.cwd_custom = expand_env(cfg.cwd_custom, cfg);

  if (cfg.window_timeout_ms == 0) cfg.window_timeout_ms = 15000;
  logf("config: args=%d tail=[%s] custom=[%s]", (int)cfg.args, utf8(cfg.args == ArgMode::All ? L"" : tail_override()).c_str(), cfg.args_custom.c_str());
  return cfg;
}

std::string config_summary(const Config& cfg) {
  std::string s;
  s += "ini=" + cfg.ini_path + (cfg.loaded ? " (found)" : " (missing)");
  s += " target=" + utf8(cfg.target);
  s += " desktop=" + (cfg.has_desktop ? cfg.desktop : std::string("<none>"));
  s += std::string(" mode=") + (cfg.switch_to ? "switch" : "background");
  s += std::string(" create=") + (cfg.create ? "true" : "false");
  s += " rule=" + (cfg.matched_rule.empty() ? std::string("<none>") : cfg.matched_rule);
  return s;
}

}  // namespace vd
