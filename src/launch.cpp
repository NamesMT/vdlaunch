#include "launch.h"
#include "desktop.h"
#include "util.h"
#include <tlhelp32.h>
#include <vector>
#include <algorithm>

namespace vd {

// Windows wants argv[0] quoted when the path may contain spaces. We normalize
// it to the real target so scripts that inspect argv[0] see the app they asked for.
static std::wstring quote_arg(const std::wstring& a) {
  if (!a.empty() && a.find_first_of(L" \t\n\v\"") == std::wstring::npos) return a;
  std::wstring out = L"\"";
  for (size_t i = 0; i < a.size(); i++) {
    size_t slashes = 0;
    while (i < a.size() && a[i] == L'\\') { slashes++; i++; }
    if (i == a.size()) { out.append(slashes * 2, L'\\'); break; }
    if (a[i] == L'"') { out.append(slashes * 2 + 1, L'\\'); out += L'"'; }
    else { out.append(slashes, L'\\'); out += a[i]; }
  }
  out += L"\"";
  return out;
}

bool build_plan(const Config& cfg, const std::wstring& raw_command_line, LaunchPlan* plan, std::string* error) {
  if (cfg.target.empty()) {
    *error = "no target executable configured";
    return false;
  }
  if (!file_exists(cfg.target)) {
    *error = "target not found: " + utf8(cfg.target);
    return false;
  }

  plan->target = cfg.target;
  std::wstring line = quote_arg(cfg.target);

  bool same_file = iequalsw(path_abs(plan->target), path_abs(exe_path()));
  if (same_file) {
    *error = "target resolves to the launcher itself: " + utf8(plan->target);
    return false;
  }

  switch (cfg.args) {
    case ArgMode::Custom:
      if (!cfg.args_custom.empty()) line += L" " + wide(cfg.args_custom);
      else if (!tail_override().empty()) line += L" " + tail_override();
      break;
    case ArgMode::All:
      // The caller's whole command line, including our own path.
      line = raw_command_line;
      break;
    case ArgMode::None:
      break;
  }

  plan->cmdline = line;

  switch (cfg.cwd) {
    case CwdMode::Target:
      plan->workdir = path_dir(cfg.target);
      plan->use_workdir = dir_exists(plan->workdir);
      break;
    case CwdMode::Custom:
      plan->workdir = cfg.cwd_custom;
      plan->use_workdir = dir_exists(plan->workdir);
      break;
    case CwdMode::Inherit:
      plan->use_workdir = false;
      break;
  }
  return true;
}

static std::vector<wchar_t> build_env_block(const Config& cfg) {
  std::vector<std::wstring> items;

  if (LPWCH raw = GetEnvironmentStringsW()) {
    for (LPWCH p = raw; *p;) {
      std::wstring e = p;
      p += e.size() + 1;
      if (e.empty()) continue;
      if (e[0] == L'=') { items.push_back(e); continue; }  // hidden drive-current vars
      size_t eq = e.find(L'=');
      std::wstring name = eq == std::wstring::npos ? e : e.substr(0, eq);
      bool skip = false;
      for (auto& u : cfg.env_unset) if (iequalsw(u, name)) { skip = true; break; }
      if (skip) continue;
      for (auto& s : cfg.env_set) if (iequalsw(s.first, name)) { skip = true; break; }
      if (skip) continue;
      items.push_back(e);
    }
    FreeEnvironmentStringsW(raw);
  }

  // Later entries win, so drop any earlier definition of the same name.
  for (auto& s : cfg.env_set) {
    items.erase(std::remove_if(items.begin(), items.end(), [&](const std::wstring& e) {
      size_t eq = e.find(L'=');
      return eq != std::wstring::npos && iequalsw(e.substr(0, eq), s.first);
    }), items.end());
    items.push_back(s.first + L"=" + s.second);
  }
  for (auto& u : cfg.env_unset) {
    items.erase(std::remove_if(items.begin(), items.end(), [&](const std::wstring& e) {
      size_t eq = e.find(L'=');
      return eq != std::wstring::npos && iequalsw(e.substr(0, eq), u);
    }), items.end());
    items.push_back(u + L"=");
  }
  std::sort(items.begin(), items.end());

  std::vector<wchar_t> block;
  for (auto& i : items) { block.insert(block.end(), i.begin(), i.end()); block.push_back(L'\0'); }
  block.push_back(L'\0');
  return block;
}

struct WindowSearch {
  std::vector<DWORD> pids;
  HWND               found = nullptr;
};

static BOOL CALLBACK enum_proc(HWND h, LPARAM lp) {
  auto* s = (WindowSearch*)lp;
  DWORD pid = 0;
  GetWindowThreadProcessId(h, &pid);
  if (std::find(s->pids.begin(), s->pids.end(), pid) == s->pids.end()) return TRUE;
  if (!IsWindowVisible(h)) return TRUE;
  if (GetWindow(h, GW_OWNER)) return TRUE;
  wchar_t cls[64] = {};
  GetClassNameW(h, cls, 64);
  if (!wcslen(cls)) return TRUE;
  s->found = h;
  return FALSE;
}

static void add_child_pids(DWORD parent, std::vector<DWORD>* pids) {
  HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
  if (snap == INVALID_HANDLE_VALUE) return;
  PROCESSENTRY32W pe = {};
  pe.dwSize = sizeof(pe);
  if (Process32FirstW(snap, &pe)) {
    do {
      if (pe.th32ParentProcessID == parent &&
          std::find(pids->begin(), pids->end(), pe.th32ProcessID) == pids->end())
        pids->push_back(pe.th32ProcessID);
    } while (Process32NextW(snap, &pe));
  }
  CloseHandle(snap);
}

static HWND wait_for_window(const std::vector<DWORD>& roots, int timeout_ms) {
  WindowSearch s;
  s.pids = roots;
  ULONGLONG start = GetTickCount64();
  ULONGLONG next_expand = 0;
  while (GetTickCount64() - start < (ULONGLONG)timeout_ms) {
    s.found = nullptr;
    EnumWindows(enum_proc, (LPARAM)&s);
    if (s.found) return s.found;
    if (GetTickCount64() >= next_expand) {
      size_t before = s.pids.size();
      for (size_t i = 0; i < before; i++) add_child_pids(s.pids[i], &s.pids);
      next_expand = GetTickCount64() + 400;
    }
    Sleep(50);
  }
  return nullptr;
}

static bool target_is_console(const std::wstring& path) {
  HANDLE f = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
  if (f == INVALID_HANDLE_VALUE) return false;
  IMAGE_DOS_HEADER dos = {};
  DWORD got = 0;
  bool console = false;
  if (ReadFile(f, &dos, sizeof(dos), &got, nullptr) && dos.e_magic == IMAGE_DOS_SIGNATURE) {
    SetFilePointer(f, dos.e_lfanew, nullptr, FILE_BEGIN);
    IMAGE_NT_HEADERS nt = {};
    if (ReadFile(f, &nt, sizeof(nt), &got, nullptr) && nt.Signature == IMAGE_NT_SIGNATURE)
      console = nt.OptionalHeader.Subsystem == IMAGE_SUBSYSTEM_WINDOWS_CUI;
  }
  CloseHandle(f);
  return console;
}

int run(const Config& cfg, const std::wstring& raw_command_line) {
  LaunchPlan plan;
  std::string error;
  if (!build_plan(cfg, raw_command_line, &plan, &error)) {
    logf("launch: %s", error.c_str());
    if (!cfg.quiet) {
      std::wstring msg = L"vdlaunch cannot start the application.\n\n" + wide(error) +
                         L"\n\nLauncher: " + exe_path() + L"\nConfig: " + wide(cfg.ini_path);
      MessageBoxW(nullptr, msg.c_str(), L"vdlaunch", MB_OK | MB_ICONERROR | MB_SETFOREGROUND);
    }
    return 2;
  }

  logf("launch: cmd=%s", utf8(plan.cmdline).c_str());
  if (plan.use_workdir) logf("launch: cwd=%s", utf8(plan.workdir).c_str());

  int wanted = 0;
  bool want_window_move = false;
  if (cfg.has_desktop && vd::init()) {
    ResolvedDesktop rd = vd::resolve(cfg.desktop, cfg.create);
    if (!rd.ok) {
      logf("launch: desktop resolve failed: %s", rd.error.c_str());
      if (!cfg.quiet)
        MessageBoxW(nullptr, (L"vdlaunch could not resolve the desktop \"" + wide(cfg.desktop) + L"\":\n\n" +
                              wide(rd.error) + L"\n\nLaunching without virtual-desktop control.").c_str(),
                    L"vdlaunch", MB_OK | MB_ICONWARNING | MB_SETFOREGROUND);
    } else {
      wanted = rd.index;
      if (rd.created) logf("launch: desktop %d was created for this launch", rd.index);
      if (cfg.switch_to) {
        vd::switch_desktop(wanted);
        logf("launch: switched to desktop %d", wanted);
      } else {
        want_window_move = true;
      }
    }
  } else if (cfg.has_desktop) {
    logf("launch: virtual desktops unavailable; launching normally");
  }

  auto env = build_env_block(cfg);
  STARTUPINFOW si = {};
  si.cb = sizeof(si);
  PROCESS_INFORMATION pi = {};

  std::wstring mutable_cmd = plan.cmdline;
  BOOL ok = CreateProcessW(plan.target.c_str(), mutable_cmd.empty() ? nullptr : &mutable_cmd[0],
                           nullptr, nullptr, FALSE, CREATE_UNICODE_ENVIRONMENT,
                           env.data(),
                           plan.use_workdir ? plan.workdir.c_str() : nullptr,
                           &si, &pi);
  if (!ok) {
    DWORD err = GetLastError();
    logf("launch: CreateProcessW failed (%lu)", (unsigned long)err);
    if (!cfg.quiet)
      MessageBoxW(nullptr, (L"vdlaunch could not start:\n" + plan.target + L"\n\nError " +
                            std::to_wstring(err)).c_str(), L"vdlaunch", MB_OK | MB_ICONERROR | MB_SETFOREGROUND);
    return 3;
  }
  CloseHandle(pi.hThread);

  if (want_window_move && wanted > 0) {
    std::vector<DWORD> pids{pi.dwProcessId};
    HWND hwnd = wait_for_window(pids, cfg.window_timeout_ms);
    if (hwnd) {
      bool moved = vd::move_window(hwnd, wanted);
      logf("launch: window %p -> desktop %d : %s", (void*)hwnd, wanted, moved ? "moved" : "failed");
    } else {
      logf("launch: no window after %dms; left on the current desktop", cfg.window_timeout_ms);
    }
  }

  WaitMode w = cfg.wait;
  if (w == WaitMode::Auto) {
    bool has_console = GetConsoleWindow() != nullptr;
    w = (has_console || target_is_console(plan.target)) ? WaitMode::Always : WaitMode::Never;
  }

  DWORD code = 0;
  if (w == WaitMode::Always) {
    WaitForSingleObject(pi.hProcess, INFINITE);
    GetExitCodeProcess(pi.hProcess, &code);
  }
  CloseHandle(pi.hProcess);
  vd::shutdown();
  return (int)code;
}

}  // namespace vd
