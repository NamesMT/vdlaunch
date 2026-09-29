#include "creator.h"
#include "version.h"
#include "util.h"
#include <windows.h>
#include <shellapi.h>
#include <cstdio>
#include <cstring>
#include <string>

namespace vd {

static const int kRes64 = 1001;
static const int kRes32 = 1002;

static std::wstring trimw(std::wstring s) {
  size_t a = 0, b = s.size();
  while (a < b && (s[a] == L' ' || s[a] == L'\t')) a++;
  while (b > a && (s[b - 1] == L' ' || s[b - 1] == L'\t')) b--;
  return s.substr(a, b - a);
}

static std::wstring read_line() {
  wchar_t buf[1024];
  if (!fgetws(buf, 1024, stdin)) return L"";
  std::wstring s = buf;
  while (!s.empty() && (s.back() == L'\n' || s.back() == L'\r')) s.pop_back();
  return trimw(s);
}

// Blank keeps the launcher default, so the key is left out of the ini.
static bool parse_bool(const std::wstring& in, bool* out) {
  std::string s = lower(utf8(in));
  if (s.empty()) return false;
  if (s == "y" || s == "yes" || s == "true" || s == "1" || s == "on") { *out = true; return true; }
  if (s == "n" || s == "no" || s == "false" || s == "0" || s == "off") { *out = false; return true; }
  return false;
}

static unsigned short pe_machine_of(const std::wstring& path) {
  HANDLE f = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                         nullptr, OPEN_EXISTING, 0, nullptr);
  if (f == INVALID_HANDLE_VALUE) return 0;
  IMAGE_DOS_HEADER dos = {};
  DWORD got = 0;
  unsigned short machine = 0;
  if (ReadFile(f, &dos, sizeof(dos), &got, nullptr) && dos.e_magic == IMAGE_DOS_SIGNATURE) {
    SetFilePointer(f, dos.e_lfanew, nullptr, FILE_BEGIN);
    DWORD sig = 0;
    if (ReadFile(f, &sig, sizeof(sig), &got, nullptr) && sig == IMAGE_NT_SIGNATURE) {
      IMAGE_FILE_HEADER fh = {};
      if (ReadFile(f, &fh, sizeof(fh), &got, nullptr)) machine = fh.Machine;
    }
  }
  CloseHandle(f);
  return machine;
}

// Cheap guard against wrapping a wrapper and clobbering the real app's backup.
// The marker is matched with std::string::find, not strstr: a PE is full of NUL
// bytes and strstr would stop at the first one.
static bool looks_like_launcher(const std::wstring& path) {
  HANDLE f = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
  if (f == INVALID_HANDLE_VALUE) return false;
  const char needle[] = "vdlaunch-wrapper";
  const size_t nlen = sizeof(needle) - 1;
  char buf[65536];
  DWORD got = 0;
  std::string data;
  bool found = false;
  while (!found && ReadFile(f, buf, sizeof(buf), &got, nullptr) && got > 0) {
    data.append(buf, got);
    if (data.find(needle) != std::string::npos) found = true;
    // Keep only enough tail to catch a marker spanning a read boundary.
    else if (data.size() > nlen) data.erase(0, data.size() - nlen);
  }
  CloseHandle(f);
  return found;
}

static bool extract_resource(int id, const std::wstring& out, std::string* err) {
  HMODULE self = GetModuleHandleW(nullptr);
  HRSRC res = FindResourceW(self, MAKEINTRESOURCEW(id), (LPCWSTR)RT_RCDATA);
  if (!res) { *err = "embedded launcher missing from this build"; return false; }
  HGLOBAL h = LoadResource(self, res);
  if (!h) { *err = "cannot load embedded launcher"; return false; }
  const void* p = LockResource(h);
  DWORD size = SizeofResource(self, res);
  if (!p || !size) { *err = "embedded launcher is empty"; return false; }

  HANDLE f = CreateFileW(out.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                         FILE_ATTRIBUTE_NORMAL, nullptr);
  if (f == INVALID_HANDLE_VALUE) {
    *err = "cannot write " + utf8(out) + " (error " + std::to_string(GetLastError()) + ")";
    return false;
  }
  DWORD written = 0;
  bool ok = WriteFile(f, p, size, &written, nullptr) && written == size;
  FlushFileBuffers(f);
  CloseHandle(f);
  if (!ok) { *err = "short write to " + utf8(out); DeleteFileW(out.c_str()); return false; }
  return true;
}

static bool write_ini(const std::wstring& path, const CreatorOptions& opt,
                      const std::wstring& target, std::string* err) {
  std::string ini = "[launch]\r\n";
  ini += "target = " + utf8(target) + "\r\n";
  if (!opt.desktop.empty()) ini += "desktop = " + opt.desktop + "\r\n";
  if (opt.have_switch) ini += std::string("switch = ") + (opt.switch_to ? "true" : "false") + "\r\n";
  if (opt.have_create) ini += std::string("create = ") + (opt.create ? "true" : "false") + "\r\n";
  if (!opt.wait.empty()) ini += "wait = " + opt.wait + "\r\n";

  HANDLE f = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                         FILE_ATTRIBUTE_NORMAL, nullptr);
  if (f == INVALID_HANDLE_VALUE) {
    *err = "cannot write vdlaunch.ini (error " + std::to_string(GetLastError()) + ")";
    return false;
  }
  DWORD written = 0;
  bool ok = WriteFile(f, ini.data(), (DWORD)ini.size(), &written, nullptr) && written == ini.size();
  CloseHandle(f);
  if (!ok) *err = "short write to vdlaunch.ini";
  return ok;
}

void creator_usage() {
  printf("vdlaunchCreator %s\n\n", VDLAUNCH_VERSION);
  printf("Wrap an app in this folder so it starts on a chosen virtual desktop.\n\n");
  printf("  vdlaunchCreator.exe [options]\n\n");
  printf("  --name <name>      program name without .exe (prompted if absent)\n");
  printf("  --desktop <spec>   2, +1, -1, current, new (prompted if absent)\n");
  printf("  --switch <bool>    switch the visible desktop first\n");
  printf("  --create <bool>    create the desktop when it is missing\n");
  printf("  --wait <mode>      auto, always, never\n");
  printf("  --dir <path>       folder to work in (default: this exe's folder)\n");
  printf("  --dry-run          show what would happen, change nothing\n");
  printf("  --force            overwrite an existing _name.exe (dangerous)\n");
  printf("  -y, --yes          do not ask for confirmation\n");
  printf("  -h, --help         this text\n");
}

void attach_parent_console_creator() {
  if (GetConsoleWindow()) return;
  if (AttachConsole(ATTACH_PARENT_PROCESS)) {
    FILE* f;
    freopen_s(&f, "CONOUT$", "w", stdout);
    freopen_s(&f, "CONOUT$", "w", stderr);
  }
}

int creator_run(CreatorOptions& opt) {
  std::wstring dir = opt.dir.empty() ? exe_dir() : path_abs(opt.dir);
  if (!dir_exists(dir)) {
    printf("Folder not found: %ls\n", dir.c_str());
    return 2;
  }
  bool interactive = !opt.assume_yes;
  bool prompt_name = opt.name.empty();
  bool prompt_desktop = opt.desktop.empty() && !opt.assume_yes;

  printf("vdlaunchCreator %s\n\n", VDLAUNCH_VERSION);
  printf("Folder: %ls\n\n", dir.c_str());

  if (prompt_name) {
    for (;;) {
      printf("Program name (without .exe): ");
      opt.name = read_line();
      if (opt.name.empty()) { printf("  Please enter a name.\n"); continue; }
      break;
    }
  }
  std::wstring name = opt.name;
  if (name.size() > 4 && iequalsw(name.substr(name.size() - 4), L".exe"))
    name = name.substr(0, name.size() - 4);
  if (name.empty() || name.find_first_of(L"\\/:*?\"<>|") != std::wstring::npos) {
    printf("Invalid program name: %ls\n", name.c_str());
    return 2;
  }

  std::wstring target = path_join(dir, name + L".exe");
  std::wstring backup = path_join(dir, L"_" + name + L".exe");
  std::wstring ini    = path_join(dir, L"vdlaunch.ini");

  if (!file_exists(target)) {
    printf("\nNot found: %ls\n", target.c_str());
    printf("Put vdlaunchCreator.exe in the folder that holds %ls.exe and run it again.\n", name.c_str());
    return 2;
  }

  if (looks_like_launcher(target)) {
    printf("\n%ls already looks like a vdlaunch wrapper.\n", target.c_str());
    printf("Nothing to do - the real app should be at %ls.\n", backup.c_str());
    return 2;
  }
  if (file_exists(backup) && !opt.force) {
    printf("\n%ls already exists, so wrapping would overwrite a previous backup.\n", backup.c_str());
    printf("Remove or rename it first, or pass --force to overwrite it.\n");
    return 2;
  }

  unsigned short machine = pe_machine_of(target);
  bool is32 = machine == IMAGE_FILE_MACHINE_I386;
  int res = is32 ? kRes32 : kRes64;
  const char* arch = is32 ? "x86" : (machine == IMAGE_FILE_MACHINE_AMD64 ? "x64" : "x64/other");

  if (prompt_desktop) {
    printf("Desktop to launch on [2, +1, -1, current, new, blank = new]: ");
    opt.desktop = utf8(read_line());
  }
  if (interactive) {
    std::wstring in;
    if (!opt.have_switch) {
      printf("Switch the visible desktop first? [y/N, blank = keep default]: ");
      in = read_line();
      if (parse_bool(in, &opt.switch_to)) opt.have_switch = true;
    }
    if (!opt.have_create) {
      printf("Create the desktop if it does not exist? [Y/n, blank = keep default]: ");
      in = read_line();
      if (parse_bool(in, &opt.create)) opt.have_create = true;
    }
    if (opt.wait.empty()) {
      printf("Wait for the app to exit? [auto/always/never, blank = auto]: ");
      std::string w = lower(utf8(read_line()));
      if (w == "auto" || w == "always" || w == "never") opt.wait = w;
    }
  }

  std::wstring ini_backup = ini + L".bak";
  bool ini_existed = file_exists(ini);

  printf("\nPlan\n");
  printf("  %-22ls -> _%ls.exe\n", (name + L".exe").c_str(), name.c_str());
  printf("  write %-15ls (%s launcher)\n", (name + L".exe").c_str(), arch);
  printf("  write vdlaunch.ini      desktop=%s%s%s%s\n",
         opt.desktop.empty() ? "<default: new>" : opt.desktop.c_str(),
         opt.have_switch ? (opt.switch_to ? " switch=true" : " switch=false") : "",
         opt.have_create ? (opt.create ? " create=true" : " create=false") : "",
         opt.wait.empty() ? "" : (" wait=" + opt.wait).c_str());
  if (ini_existed) printf("  keep the current vdlaunch.ini as vdlaunch.ini.bak\n");
  if (file_exists(backup)) printf("  ! overwriting the existing %ls\n", backup.c_str());

  if (opt.dry_run) { printf("\nDry run: nothing changed.\n"); return 0; }

  if (interactive) {
    printf("\nProceed? [Y/n]: ");
    std::wstring in = read_line();
    bool yes = true;
    if (!in.empty() && !parse_bool(in, &yes)) yes = false;
    if (!yes) { printf("Cancelled.\n"); return 1; }
  }

  // Extract the wrapper before the rename, so a failure leaves the app untouched.
  std::wstring staged = target + L".vdlnew";
  DeleteFileW(staged.c_str());
  std::string err;
  if (!extract_resource(res, staged, &err)) {
    printf("\nFailed: %s\n", err.c_str());
    DeleteFileW(staged.c_str());
    return 3;
  }

  if (ini_existed && !MoveFileExW(ini.c_str(), ini_backup.c_str(), MOVEFILE_REPLACE_EXISTING)) {
    printf("\nFailed to back up vdlaunch.ini (error %lu); nothing changed.\n", GetLastError());
    DeleteFileW(staged.c_str());
    return 3;
  }

  if (!MoveFileW(target.c_str(), backup.c_str())) {
    printf("\nFailed to rename %ls (error %lu); nothing changed.\n", target.c_str(), GetLastError());
    DeleteFileW(staged.c_str());
    return 3;
  }
  if (!MoveFileW(staged.c_str(), target.c_str())) {
    DWORD e = GetLastError();
    MoveFileW(backup.c_str(), target.c_str());          // put the app back
    DeleteFileW(staged.c_str());
    printf("\nFailed to place the launcher (error %lu). Your app was restored.\n", e);
    return 3;
  }

  if (!write_ini(ini, opt, L"_" + name + L".exe", &err)) {
    DeleteFileW(target.c_str());
    MoveFileW(backup.c_str(), target.c_str());          // full rollback
    if (ini_existed) MoveFileExW(ini_backup.c_str(), ini.c_str(), MOVEFILE_REPLACE_EXISTING);
    printf("\nFailed: %s\nYour app was restored.\n", err.c_str());
    return 3;
  }

  printf("\nDone. %ls now starts on %s.\n", target.c_str(),
         opt.desktop.empty() ? "a new desktop" : opt.desktop.c_str());
  printf("Run %ls --diag if it does not behave.\n", (name + L".exe").c_str());
  return 0;
}

}  // namespace vd
