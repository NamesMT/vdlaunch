#include "auto_dispatch.h"
#include "config.h"
#include "util.h"
#include <cstdio>

namespace vd {

unsigned short pe_machine(const std::wstring& path) {
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

#if VDLAUNCH_AUTO
static const char* arch_name(unsigned short m) {
  switch (m) {
    case IMAGE_FILE_MACHINE_I386:  return "x86";
    case IMAGE_FILE_MACHINE_AMD64: return "x64";
    case IMAGE_FILE_MACHINE_ARM64: return "arm64";
    default: return "unknown";
  }
}
#endif

bool auto_dispatch_needed(const std::wstring& target) {
#if VDLAUNCH_AUTO
  if (target.empty()) return false;
  return pe_machine(target) == IMAGE_FILE_MACHINE_I386;
#else
  (void)target;
  return false;
#endif
}

int auto_dispatch(const std::wstring& target, const std::wstring& raw_command_line) {
#if VDLAUNCH_AUTO
  unsigned short target_machine = pe_machine(target);
  if (target_machine != IMAGE_FILE_MACHINE_I386) return -1;  // not our job

  std::wstring helper = path_join(exe_dir(), L"vdlaunch32.exe");
  if (!file_exists(helper)) {
    logf("auto: target is %s but %s is missing; launching natively",
         arch_name(target_machine), utf8(helper).c_str());
    return -1;
  }

  // Hand the whole thing over: the 32-bit build re-resolves the same config.
  // The helper must not re-derive the target from its own file name.
  std::wstring cmd = L"\"" + helper + L"\" --target \"" + target + L"\"";
  if (!tail_override().empty()) cmd += L" " + tail_override();
  logf("auto: delegating to %s", utf8(helper).c_str());

  STARTUPINFOW si = {};
  si.cb = sizeof(si);
  PROCESS_INFORMATION pi = {};
  std::wstring mut = cmd;
  if (!CreateProcessW(helper.c_str(), &mut[0], nullptr, nullptr, TRUE, 0, nullptr, nullptr, &si, &pi)) {
    logf("auto: CreateProcessW failed (%lu)", (unsigned long)GetLastError());
    return -1;
  }
  CloseHandle(pi.hThread);
  WaitForSingleObject(pi.hProcess, 20000);
  DWORD code = 0;
  GetExitCodeProcess(pi.hProcess, &code);
  CloseHandle(pi.hProcess);
  return (int)code;
#else
  (void)target; (void)raw_command_line;
  return -1;
#endif
}

}  // namespace vd
