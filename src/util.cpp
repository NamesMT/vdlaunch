#include "util.h"
#include <shlwapi.h>
#include <cstdarg>
#include <cstdio>
#include <cstring>

static std::wstring  g_log_path;
static bool          g_log_ready = false;
static CRITICAL_SECTION g_log_lock;
static bool          g_log_locked = false;

std::string utf8(const std::wstring& w) {
  if (w.empty()) return {};
  int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), nullptr, 0, nullptr, nullptr);
  std::string out(n, 0);
  WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), &out[0], n, nullptr, nullptr);
  return out;
}

std::wstring wide(const std::string& s) {
  if (s.empty()) return {};
  int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
  std::wstring out(n, 0);
  MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), &out[0], n);
  return out;
}

std::wstring exe_path() {
  static std::wstring cached;
  if (!cached.empty()) return cached;
  std::vector<wchar_t> buf(MAX_PATH);
  for (;;) {
    DWORD n = GetModuleFileNameW(nullptr, buf.data(), (DWORD)buf.size());
    if (n == 0) return L"";
    if (n < buf.size() - 1) { cached.assign(buf.data(), n); return cached; }
    buf.resize(buf.size() * 2);
  }
}

std::wstring exe_dir()  { return path_dir(exe_path()); }
std::wstring exe_name() {
  std::wstring p = exe_path();
  size_t s = p.find_last_of(L"\\/");
  return s == std::wstring::npos ? p : p.substr(s + 1);
}

std::wstring path_join(const std::wstring& a, const std::wstring& b) {
  if (a.empty()) return b;
  if (b.empty()) return a;
  std::wstring r = a;
  if (r.back() != L'\\' && r.back() != L'/') r += L'\\';
  return r + b;
}

std::wstring path_dir(const std::wstring& p) {
  size_t s = p.find_last_of(L"\\/");
  return s == std::wstring::npos ? L"" : p.substr(0, s);
}

std::wstring path_abs(const std::wstring& p) {
  if (p.empty()) return p;
  if (p.size() > 2 && p[1] == L':' && (p[2] == L'\\' || p[2] == L'/')) return p;

  // Command-line paths resolve against the caller's directory, like a plain
  // CreateProcess would, and only fall back to the launcher's own folder.
  std::wstring bases[2];
  wchar_t cwd[MAX_PATH] = {};
  if (GetCurrentDirectoryW(MAX_PATH, cwd)) bases[0] = cwd;
  if (p.size() > 1 && (p[0] == L'\\' || p[0] == L'/')) { bases[0].clear(); bases[1].clear(); }

  for (int i = 0; i < 2; i++) {
    std::wstring full = bases[i].empty() ? p : path_join(bases[i], p);
    std::vector<wchar_t> buf(MAX_PATH * 2);
    DWORD n = GetFullPathNameW(full.c_str(), (DWORD)buf.size(), buf.data(), nullptr);
    if (n > 0 && n < buf.size()) {
      std::wstring cand(buf.data(), n);
      if (file_exists(cand)) return cand;
      if (i == 1) return cand;
    }
    bases[i] = exe_dir();
  }
  return path_join(exe_dir(), p);
}

bool file_exists(const std::wstring& p) {
  DWORD a = GetFileAttributesW(p.c_str());
  return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY);
}

bool dir_exists(const std::wstring& p) {
  DWORD a = GetFileAttributesW(p.c_str());
  return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY);
}

void log_open(const std::wstring& path) {
  g_log_path = path;
  InitializeCriticalSection(&g_log_lock);
  g_log_locked = true;
  g_log_ready = true;
}

void log_line(const std::string& msg) {
  if (!g_log_ready) return;
  if (g_log_locked) EnterCriticalSection(&g_log_lock);
  FILE* f = _wfopen(g_log_path.c_str(), L"ab");
  if (f) {
    SYSTEMTIME st; GetLocalTime(&st);
    char head[64];
    snprintf(head, sizeof(head), "%04d-%02d-%02d %02d:%02d:%02d ", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
    std::string line = std::string(head) + msg + "\r\n";
    fwrite(line.data(), 1, line.size(), f);
    fclose(f);
  }
  if (g_log_locked) LeaveCriticalSection(&g_log_lock);
}

void log_line(const std::wstring& msg) { log_line(utf8(msg)); }

void logf(const char* fmt, ...) {
  char buf[4096];
  va_list ap; va_start(ap, fmt);
  vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);
  log_line(std::string(buf));
}

void logw(const wchar_t* fmt, ...) {
  wchar_t buf[4096];
  va_list ap; va_start(ap, fmt);
  _vsnwprintf_s(buf, _TRUNCATE, fmt, ap);
  va_end(ap);
  log_line(utf8(buf));
}

std::string trim(const std::string& s) {
  size_t a = 0, b = s.size();
  while (a < b && (unsigned char)s[a] <= ' ') a++;
  while (b > a && (unsigned char)s[b - 1] <= ' ') b--;
  return s.substr(a, b - a);
}

std::string lower(std::string s) {
  for (auto& c : s) c = (char)tolower((unsigned char)c);
  return s;
}

bool iequals(const std::string& a, const std::string& b) {
  if (a.size() != b.size()) return false;
  for (size_t i = 0; i < a.size(); i++)
    if (tolower((unsigned char)a[i]) != tolower((unsigned char)b[i])) return false;
  return true;
}

bool iequalsw(const std::wstring& a, const std::wstring& b) {
  if (a.size() != b.size()) return false;
  for (size_t i = 0; i < a.size(); i++)
    if (towlower(a[i]) != towlower(b[i])) return false;
  return true;
}

std::vector<std::string> split(const std::string& s, char sep) {
  std::vector<std::string> out;
  std::string cur;
  for (char c : s) {
    if (c == sep) { out.push_back(cur); cur.clear(); }
    else cur += c;
  }
  out.push_back(cur);
  return out;
}

// RtlGetVersion reports the real build even when the app is manifested for
// an older Windows; GetVersionEx does not.
int windows_build() {
  typedef LONG(WINAPI* PFN_RtlGetVersion)(PRTL_OSVERSIONINFOW);
  HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
  if (!ntdll) return -1;
  auto fn = reinterpret_cast<PFN_RtlGetVersion>(reinterpret_cast<void*>(GetProcAddress(ntdll, "RtlGetVersion")));
  if (!fn) return -1;
  RTL_OSVERSIONINFOW vi = {};
  vi.dwOSVersionInfoSize = sizeof(vi);
  if (fn(&vi) != 0) return -1;
  return (int)vi.dwBuildNumber;
}

bool glob_match(const std::wstring& pat, const std::wstring& txt) {
  size_t p = 0, t = 0, star = std::wstring::npos, mark = 0;
  while (t < txt.size()) {
    if (p < pat.size() && (pat[p] == L'?' || towlower(pat[p]) == towlower(txt[t]))) { p++; t++; }
    else if (p < pat.size() && pat[p] == L'*') { star = p++; mark = t; }
    else if (star != std::wstring::npos) { p = star + 1; t = ++mark; }
    else return false;
  }
  while (p < pat.size() && pat[p] == L'*') p++;
  return p == pat.size();
}
