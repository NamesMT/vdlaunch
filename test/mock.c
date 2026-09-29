/* Mock launch target for test/run.sh: records what it was handed and, in GUI
 * mode, which virtual desktop its window ended up on. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <objbase.h>
#include <servprov.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

static const GUID CLSID_ImmersiveShell = {0xC2F03A33,0x21F5,0x47FA,{0xB4,0xBB,0x15,0x63,0x62,0xA2,0xF2,0x39}};
static const GUID IID_IServiceProvider_ = {0x6D5140C1,0x7436,0x11CE,{0x80,0x34,0x00,0xAA,0x00,0x60,0x09,0xFA}};
static const GUID IID_VDManagerPublic   = {0xA5CD92FF,0x29BE,0x454C,{0x8D,0x04,0xD8,0x28,0x79,0xFB,0x3F,0x1B}};

static unsigned long com_release(void* p) {
  typedef unsigned long(STDMETHODCALLTYPE * PFN_rel)(void*);
  return ((PFN_rel)(*(void***)p)[2])(p);
}

static void append_file(const char* path, const char* line) {
  FILE* f = fopen(path, "ab");
  if (f) { fputs(line, f); fputc('\n', f); fclose(f); }
}

/* Callers hold several of these at once, so each needs its own buffer. */
static const char* env_or(const char* name, const char* dflt, char* buf, size_t cap) {
  DWORD n = GetEnvironmentVariableA(name, buf, (DWORD)cap);
  return (n > 0 && n < cap) ? buf : dflt;
}

static LRESULT CALLBACK wnd_proc(HWND h, UINT m, WPARAM w, LPARAM l) {
  if (m == WM_CLOSE || m == WM_DESTROY) { PostQuitMessage(0); return 0; }
  return DefWindowProcA(h, m, w, l);
}

static void report_desktop(const char* path, HWND hwnd) {
  void* sp = NULL;
  HRESULT hr = CoCreateInstance(&CLSID_ImmersiveShell, NULL, CLSCTX_LOCAL_SERVER,
                                &IID_IServiceProvider_, &sp);
  if (FAILED(hr) || !sp) { append_file(path, "desktop=unavailable"); return; }
  {
    typedef HRESULT(STDMETHODCALLTYPE * PFN_QS)(void*, const GUID*, const GUID*, void**);
    void* vdm = NULL;
    hr = ((PFN_QS)(*(void***)sp)[3])(sp, &IID_VDManagerPublic, &IID_VDManagerPublic, &vdm);
    if (FAILED(hr) || !vdm) { append_file(path, "desktop=unavailable"); return; }
    {
      typedef HRESULT(STDMETHODCALLTYPE * PFN_iscur)(void*, HWND, int*);
      typedef HRESULT(STDMETHODCALLTYPE * PFN_gid)(void*, HWND, GUID*);
      int on = 0;
      GUID g = {0, 0, 0, {0, 0, 0, 0, 0, 0, 0, 0}};
      HRESULT ghr;
      wchar_t gs[40] = {0};
      char line[256];
      ((PFN_iscur)(*(void***)vdm)[3])(vdm, hwnd, &on);
      ghr = ((PFN_gid)(*(void***)vdm)[4])(vdm, hwnd, &g);
      if (SUCCEEDED(ghr) && g.Data1) {
        StringFromGUID2(&g, (LPOLESTR)gs, 40);
        snprintf(line, sizeof(line), "onCurrentDesktop=%d windowDesktopId=%ls", on, gs);
      } else {
        snprintf(line, sizeof(line), "onCurrentDesktop=%d windowDesktopId=<none>", on);
      }
      append_file(path, line);
    }
    com_release(vdm);
  }
  com_release(sp);
}

int main(int argc, char** argv) {
  char outbuf[512] = {0}, tagbuf[128] = {0}, varbuf[256] = {0}, unsetbuf[256] = {0}, guibuf[16] = {0};
  const char* out = env_or("VDLAUNCH_TEST_OUT", "mock-target-out.txt", outbuf, sizeof(outbuf));
  const char* tag = env_or("VDLAUNCH_TEST_TAG", "run", tagbuf, sizeof(tagbuf));
  char buf[4096];
  int i;

  snprintf(buf, sizeof(buf), "=== %s ===", tag);
  append_file(out, buf);
  snprintf(buf, sizeof(buf), "argv0=%s", argc > 0 ? argv[0] : "");
  append_file(out, buf);
  snprintf(buf, sizeof(buf), "argc=%d", argc);
  append_file(out, buf);
  for (i = 1; i < argc; i++) {
    snprintf(buf, sizeof(buf), "arg%d=[%s]", i, argv[i]);
    append_file(out, buf);
  }
  {
    char wd[MAX_PATH] = {0};
    GetCurrentDirectoryA(MAX_PATH, wd);
    snprintf(buf, sizeof(buf), "cwd=%s", wd);
    append_file(out, buf);
    snprintf(buf, sizeof(buf), "env.TESTVAR=%s", env_or("VDLAUNCH_TEST_VAR", "<unset>", varbuf, sizeof(varbuf)));
    append_file(out, buf);
    snprintf(buf, sizeof(buf), "env.UNSETME=%s", env_or("VDLAUNCH_TEST_UNSET", "<unset>", unsetbuf, sizeof(unsetbuf)));
    append_file(out, buf);
    snprintf(buf, sizeof(buf), "rawcmdline=%s", GetCommandLineA());
    append_file(out, buf);
  }
  append_file(out, "---");

  if (env_or("VDLAUNCH_TEST_GUI", "0", guibuf, sizeof(guibuf))[0] == '1') {
    WNDCLASSA wc;
    HWND hwnd;
    memset(&wc, 0, sizeof(wc));
    wc.lpfnWndProc = wnd_proc;
    wc.hInstance = GetModuleHandleA(0);
    wc.lpszClassName = "VdLaunchMockTarget";
    RegisterClassA(&wc);
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    hwnd = CreateWindowExA(0, "VdLaunchMockTarget", "vdlaunch mock target",
                           WS_OVERLAPPEDWINDOW | WS_VISIBLE, 80, 80, 360, 220,
                           NULL, NULL, wc.hInstance, NULL);
    for (i = 0; i < 60; i++) {
      MSG m;
      while (PeekMessageA(&m, NULL, 0, 0, PM_REMOVE)) { TranslateMessage(&m); DispatchMessageA(&m); }
      Sleep(50);
    }
    report_desktop(out, hwnd);
    append_file(out, "---");
    DestroyWindow(hwnd);
    CoUninitialize();
  }
  return 0;
}
