#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <objbase.h>
#include <shobjidl.h>
#include <servprov.h>
#include <cstdio>
#include <cstring>

static const GUID CLSID_ImmersiveShell = {0xC2F03A33,0x21F5,0x47FA,{0xB4,0xBB,0x15,0x63,0x62,0xA2,0xF2,0x39}};
static const GUID SID_VDMInternal = {0xC5E0CDCA,0x7B6E,0x41B2,{0x9F,0xC4,0xD9,0x39,0x75,0xCC,0x46,0x7B}};
static const GUID IID_VDMInternal = {0x53F5CA0B,0x158F,0x4124,{0x90,0x0C,0x05,0x71,0x58,0x06,0x0B,0x27}};
static const GUID IID_IVirtualDesktop_26100 = {0x3F07F4BE,0xB107,0x441A,{0xAF,0x0F,0x39,0xD8,0x25,0x29,0x07,0x2C}};
static const GUID IID_AppViewColl = {0x1841C6D7,0x4F9D,0x42C0,{0xAF,0x41,0x87,0x47,0x53,0x8F,0x10,0xE5}};
static const GUID IID_IApplicationView = {0x372E1D3B,0x38D3,0x42E4,{0xA1,0x5B,0x8A,0xB2,0xB1,0x78,0xF5,0x13}};

static void hex(const char* tag, HRESULT hr) { printf("  [%s] hr=0x%08lX%s\n", tag, (unsigned long)hr, hr==0?" OK":""); }
static void guidstr(const GUID& g, char* out) { StringFromGUID2(g, (LPOLESTR)out, 64); }

// ---- minimal ABI: manager internal, 26100 layout (MoveViewToDesktop = slot 4)
struct VDMI : IUnknown {
  virtual HRESULT STDMETHODCALLTYPE GetCount(UINT* pCount) = 0;
  virtual HRESULT STDMETHODCALLTYPE MoveViewToDesktop(IUnknown* view, IUnknown* desktop) = 0;
  virtual HRESULT STDMETHODCALLTYPE CanViewMoveDesktops(IUnknown* view, UINT* ok) = 0;
  virtual HRESULT STDMETHODCALLTYPE GetCurrentDesktop(IUnknown** pDesktop) = 0;
  virtual HRESULT STDMETHODCALLTYPE GetDesktops(IObjectArray** arr) = 0;
  virtual HRESULT STDMETHODCALLTYPE SwitchDesktop(IUnknown* desktop) = 0;
  virtual HRESULT STDMETHODCALLTYPE SwitchDesktopAndMoveForegroundView(IUnknown* desktop) = 0;
  virtual HRESULT STDMETHODCALLTYPE CreateDesktopW(IUnknown** pDesktop) = 0;
};
struct IVD : IUnknown { virtual HRESULT STDMETHODCALLTYPE GetID(GUID* out) = 0; };
struct IAV : IUnknown {
  virtual HRESULT STDMETHODCALLTYPE GetIids(void*) = 0;
  virtual HRESULT STDMETHODCALLTYPE GetRuntimeClassName(void*) = 0;
  virtual HRESULT STDMETHODCALLTYPE GetTrustLevel(void*) = 0;
  virtual HRESULT STDMETHODCALLTYPE SetFocus() = 0;
  virtual HRESULT STDMETHODCALLTYPE SwitchTo() = 0;
  virtual HRESULT STDMETHODCALLTYPE TryInvokeBack(void*) = 0;
  virtual HRESULT STDMETHODCALLTYPE GetThumbnailWindow(HWND*) = 0;
  virtual HRESULT STDMETHODCALLTYPE GetMonitor(void*) = 0;
  virtual HRESULT STDMETHODCALLTYPE GetVisibility(UINT*) = 0;
  virtual HRESULT STDMETHODCALLTYPE SetCloak(UINT, UINT) = 0;
  virtual HRESULT STDMETHODCALLTYPE GetPosition(void*, void*) = 0;
  virtual HRESULT STDMETHODCALLTYPE SetPosition(void*) = 0;
  virtual HRESULT STDMETHODCALLTYPE InsertAfterWindow(HWND) = 0;
  virtual HRESULT STDMETHODCALLTYPE GetExtendedFramePosition(RECT*) = 0;
  virtual HRESULT STDMETHODCALLTYPE GetAppUserModelId(LPWSTR*) = 0;
  virtual HRESULT STDMETHODCALLTYPE SetAppUserModelId(LPCWSTR) = 0;
  virtual HRESULT STDMETHODCALLTYPE IsEqualByAppUserModelId(LPCWSTR, UINT*) = 0;
  virtual HRESULT STDMETHODCALLTYPE GetViewState(UINT*) = 0;
  virtual HRESULT STDMETHODCALLTYPE SetViewState(UINT) = 0;
  virtual HRESULT STDMETHODCALLTYPE GetNeediness(UINT*) = 0;
  virtual HRESULT STDMETHODCALLTYPE GetLastActivationTimestamp(ULONGLONG*) = 0;
  virtual HRESULT STDMETHODCALLTYPE SetLastActivationTimestamp(ULONGLONG) = 0;
  virtual HRESULT STDMETHODCALLTYPE GetVirtualDesktopId(GUID*) = 0;
};
struct IAVC : IUnknown {
  virtual HRESULT STDMETHODCALLTYPE GetViews(IObjectArray** a) = 0;
  virtual HRESULT STDMETHODCALLTYPE GetViewsByZOrder(IObjectArray** a) = 0;
  virtual HRESULT STDMETHODCALLTYPE GetViewsByAppUserModelId(LPCWSTR s, IObjectArray** a) = 0;
  virtual HRESULT STDMETHODCALLTYPE GetViewForHwnd(HWND hwnd, IAV** v) = 0;
};


static DWORD s_selfPid = 0;
static HWND* s_target = nullptr;
static BOOL CALLBACK enumCb(HWND h, LPARAM) {
  if (!IsWindowVisible(h)) return TRUE;
  DWORD pid = 0; GetWindowThreadProcessId(h, &pid);
  if (pid == s_selfPid) return TRUE;
  if (GetWindow(h, GW_OWNER)) return TRUE;
  char cls[256] = {0}; GetClassNameA(h, cls, sizeof(cls));
  if (!strcmp(cls, "Progman") || !strcmp(cls, "Shell_TrayWnd") || !strcmp(cls, "Windows.UI.Core.CoreWindow")) return TRUE;
  *s_target = h; return FALSE;
}

static LONG CALLBACK veh(EXCEPTION_POINTERS* ep) {
  printf("\n!! FAULT code=0x%08lX addr=%p\n", (unsigned long)ep->ExceptionRecord->ExceptionCode, ep->ExceptionRecord->ExceptionAddress);
  HMODULE m = nullptr;
  if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        (LPCSTR)ep->ExceptionRecord->ExceptionAddress, &m)) {
    char path[MAX_PATH]={0}; GetModuleFileNameA(m, path, MAX_PATH);
    printf("   faulting module: %s (base %p)\n", path, (void*)m);
  }
  fflush(stdout); return EXCEPTION_EXECUTE_HANDLER;
}
int main() {
  setvbuf(stdout, nullptr, _IONBF, 0);
  AddVectoredExceptionHandler(1, veh);
  CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
  IServiceProvider* sp = nullptr;
  HRESULT hr = CoCreateInstance(CLSID_ImmersiveShell, nullptr, CLSCTX_LOCAL_SERVER, IID_IServiceProvider, (void**)&sp);
  hex("CoCreateInstance(ImmersiveShell, IServiceProvider)", hr);
  if (FAILED(hr)) return 1;

  VDMI* mgr = nullptr;
  hr = sp->QueryService(SID_VDMInternal, IID_VDMInternal, (void**)&mgr);
  hex("QueryService(VDMInternal 26100)", hr);
  if (FAILED(hr)) return 2;

  UINT count = 0;
  hex("VDMI::GetCount", mgr->GetCount(&count));
  printf("  desktop count = %u\n", count);

  IObjectArray* arr = nullptr;
  hex("VDMI::GetDesktops", mgr->GetDesktops(&arr));
  printf("  arr=%p\n", (void*)arr); fflush(stdout);
  GUID lastDesktop = {};
  if (arr) {
    for (UINT i = 0; i < count; i++) {
      IUnknown* u = nullptr;
      HRESULT gh = arr->GetAt(i, IID_IVirtualDesktop_26100, (void**)&u);
      printf("  GetAt(%u) hr=0x%08lX u=%p\n", i, (unsigned long)gh, (void*)u); fflush(stdout);
      if (FAILED(gh) || !u) continue;
      void** vt = *(void***)u;
      printf("    vtable=%p  [0]=%p [1]=%p [2]=%p [3]=%p [4]=%p [5]=%p\n",
             (void*)vt, vt[0], vt[1], vt[2], vt[3], vt[4], vt[5]); fflush(stdout);
      GUID g = {}; 
      HRESULT hr2 = ((IVD*)u)->GetID(&g);
      char sb[64]; guidstr(g, sb);
      printf("    GetID hr=0x%08lX -> %ls\n", (unsigned long)hr2, sb); fflush(stdout);
      lastDesktop = g;
    }
  }
  IUnknown *cur = nullptr;
  if (SUCCEEDED(mgr->GetCurrentDesktop(&cur)) && cur) {
    GUID g = {}; ((IVD*)cur)->GetID(&g);
    char s[64]; guidstr(g, s); printf("  CURRENT desktop = %ls\n", s); cur->Release();
  }

  IAVC* coll = nullptr;
  hr = sp->QueryService(IID_AppViewColl, IID_AppViewColl, (void**)&coll);
  hex("QueryService(AppViewCollection)", hr);
  if (FAILED(hr)) return 3;

  // find a foreign toplevel window
  HWND target = nullptr;
  {
    s_selfPid = GetCurrentProcessId(); s_target = &target;
    EnumWindows(enumCb, 0);
  }
  printf("\n  target hwnd=%p\n", (void*)target);
  if (!target) return 4;

  IAV* view = nullptr;
  hr = coll->GetViewForHwnd(target, &view);
  hex("IAVC::GetViewForHwnd(foreign hwnd)", hr);
  if (FAILED(hr) || !view) return 5;

  GUID before = {};
  hr = view->GetVirtualDesktopId(&before);
  { char s[64]; guidstr(before, s); printf("  view BEFORE = %ls\n", s); }

  // create a fresh desktop and move the foreign window onto it
  IUnknown* fresh = nullptr;
  hr = mgr->CreateDesktopW(&fresh);
  hex("VDMI::CreateDesktopW", hr);
  if (SUCCEEDED(hr) && fresh) {
    GUID fg = {}; ((IVD*)fresh)->GetID(&fg);
    char s[64]; guidstr(fg, s); printf("  created desktop = %ls\n", s);
    hr = mgr->MoveViewToDesktop(view, fresh);
    hex("VDMI::MoveViewToDesktop(foreign view -> new desktop)", hr);
    GUID after = {};
    if (SUCCEEDED(((IAV*)view)->GetVirtualDesktopId(&after))) {
      char s2[64]; guidstr(after, s2);
      printf("  view AFTER  = %ls  => %s\n", s2, IsEqualGUID(after, fg) ? "MOVED OK" : "NOT MOVED");
    }
    fresh->Release();
  }
  printf("\nRESULT: %s\n", "probe finished");
  return 0;
}
