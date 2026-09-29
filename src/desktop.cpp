#include "desktop.h"
#include "guard.h"
#include "util.h"
#include <objbase.h>
#include <servprov.h>
#include <shobjidl.h>
#include <cstring>

namespace vd {

static const GUID CLSID_ImmersiveShell     = {0xC2F03A33,0x21F5,0x47FA,{0xB4,0xBB,0x15,0x63,0x62,0xA2,0xF2,0x39}};
static const GUID SID_VDManagerInternal    = {0xC5E0CDCA,0x7B6E,0x41B2,{0x9F,0xC4,0xD9,0x39,0x75,0xCC,0x46,0x7B}};
static const GUID IID_IVirtualDesktopRev   = {0x3F07F4BE,0xB107,0x441A,{0xAF,0x0F,0x39,0xD8,0x25,0x29,0x07,0x2C}};
static const GUID IID_IVirtualDesktopOld   = {0xFF72FFDD,0xBE7E,0x43FC,{0x9C,0x03,0xAD,0x81,0x68,0x1E,0x88,0xE4}};
static const GUID IID_IVirtualDesktop21313 = {0x536D3495,0xB208,0x4CC9,{0xAE,0x26,0xDE,0x81,0x11,0x27,0x5B,0xF8}};
static const GUID IID_AppViewCollection    = {0x1841C6D7,0x4F9D,0x42C0,{0xAF,0x41,0x87,0x47,0x53,0x8F,0x10,0xE5}};
static const GUID IID_VDManagerPublic      = {0xA5CD92FF,0x29BE,0x454C,{0x8D,0x04,0xD8,0x28,0x79,0xFB,0x3F,0x1B}};
static const GUID IID_IServiceProvider_    = {0x6D5140C1,0x7436,0x11CE,{0x80,0x34,0x00,0xAA,0x00,0x60,0x09,0xFA}};

// Per-revision method slots, from pyvda's tables. The 26100 row matches what was
// verified on hardware and winvd's published order for the same build.
// MoveViewToDesktop and GetDesktops are stable across revisions; only
// SwitchDesktop, CreateDesktopW and RemoveDesktop shift.
enum Revision { REV_26100 = 0, REV_22631, REV_22621, REV_22449, REV_21313, REV_20231, REV_LEGACY, REV_COUNT };

struct Layout {
  int get_count, move_view, get_current, get_desktops;
  int switch_desktop, create_desktop, remove_desktop;
  const GUID* desktop_iid;
  const char* revision;
};

static const Layout kLayouts[REV_COUNT] = {
  {3, 4, 6, 7,  9, 11, 13, &IID_IVirtualDesktopRev,   "26100+ (verified)"},
  {3, 4, 6, 7,  9, 10, 12, &IID_IVirtualDesktopRev,   "22631"},
  {3, 4, 6, 7,  9, 10, 12, &IID_IVirtualDesktopRev,   "22621"},
  {3, 4, 6, 7,  9, 10, 12, &IID_IVirtualDesktop21313, "22449"},
  {3, 4, 6, 7,  9, 10, 12, &IID_IVirtualDesktop21313, "21313"},
  {3, 4, 6, 7,  9, 10, 11, &IID_IVirtualDesktopOld,   "20231"},
  {3, 4, 6, 7,  9,  9, 10, &IID_IVirtualDesktopOld,   "19041-"},
};

struct IidCandidate { const GUID* iid; const char* label; int rev; };
static const GUID IID_VDMI_26100 = {0x53F5CA0B,0x158F,0x4124,{0x90,0x0C,0x05,0x71,0x58,0x06,0x0B,0x27}};
static const GUID IID_VDMI_22631 = {0x4970BA3D,0xFD4E,0x4647,{0xBE,0xA3,0xD8,0x90,0x76,0xEF,0x4B,0x9C}};
static const GUID IID_VDMI_22621 = {0xA3175F2D,0x239C,0x4BD2,{0x8A,0xA0,0xEE,0xBA,0x8B,0x0B,0x13,0x8E}};
static const GUID IID_VDMI_21313 = {0xB2F925B9,0x5A0F,0x4D2E,{0x9F,0x4D,0x2B,0x15,0x07,0x59,0x3C,0x10}};
static const GUID IID_VDMI_20231 = {0x094AFE11,0x44F2,0x4BA0,{0x97,0x6F,0x29,0xA9,0x7E,0x26,0x3E,0xE0}};
static const GUID IID_VDMI_9000  = {0xF31574D6,0xB682,0x4CDC,{0xBD,0x56,0x18,0x27,0x86,0x0A,0xBE,0xC6}};
static const IidCandidate kIids[] = {
  {&IID_VDMI_26100, "26100+", REV_26100}, {&IID_VDMI_22631, "22631", REV_22631},
  {&IID_VDMI_22621, "22621", REV_22621},  {&IID_VDMI_21313, "21313", REV_21313},
  {&IID_VDMI_20231, "20231", REV_20231},  {&IID_VDMI_9000,  "19041-", REV_LEGACY},
};

enum : int { kDesktopGetId = 4, kCollGetViewForHwnd = 6,
             kPubIsOnCurrent = 3, kPubGetId = 4, kPubMove = 5 };

typedef HRESULT(STDMETHODCALLTYPE* PFN_hwndout)(void*, HWND, void**);
typedef HRESULT(STDMETHODCALLTYPE* PFN_iscur)(void*, HWND, int*);
typedef HRESULT(STDMETHODCALLTYPE* PFN_hwndguid)(void*, HWND, GUID*);
typedef HRESULT(STDMETHODCALLTYPE* PFN_hwndmove)(void*, HWND, GUID*);

static IServiceProvider* g_sp = nullptr;
static void*             g_mgr = nullptr;
static void**            g_vt = nullptr;
static void*             g_vdm = nullptr;
static void*             g_coll = nullptr;
static bool              g_tried = false;
static bool              g_com = false;
static int               g_rev = REV_26100;
static Layout            g_layout;
static EngineInfo        g_info;

static int   count_raw();
static void* desktop_object(int index);
static int   candidates(int which, int* out);

bool available() { return init(); }

bool init() {
  if (g_tried) return g_mgr != nullptr;
  g_tried = true;
  g_info.build = windows_build();
  guard_install();

  HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
  if (hr == S_OK || hr == S_FALSE) g_com = true;
  else if (hr == RPC_E_CHANGED_MODE) g_com = false;
  else { logf("desktop: CoInitializeEx hr=0x%08lX", (unsigned long)hr); return false; }

  if (FAILED(CoCreateInstance(CLSID_ImmersiveShell, nullptr, CLSCTX_LOCAL_SERVER,
                              IID_IServiceProvider_, (void**)&g_sp)) || !g_sp) {
    log_line("desktop: ImmersiveShell unavailable");
    g_sp = nullptr;
    return false;
  }

  // Probe newest-first: a newer IID can also answer on an older build.
  for (auto& c : kIids) {
    void* m = nullptr;
    hr = g_sp->QueryService(SID_VDManagerInternal, *c.iid, &m);
    if (SUCCEEDED(hr) && m) {
      g_mgr = m;
      g_info.iid = wide(c.label);
      g_rev = c.rev;
      break;
    }
  }
  if (!g_mgr) {
    logf("desktop: no known IVirtualDesktopManagerInternal interface on build %d", g_info.build);
    return false;
  }
  g_vt = *(void***)g_mgr;
  g_layout = kLayouts[g_rev];

  if (count_raw() < 0) {
    log_line("desktop: manager layout probe failed; disabling virtual-desktop support");
    ((IUnknown*)g_mgr)->Release();
    g_mgr = nullptr;
    g_vt = nullptr;
    return false;
  }

  if (FAILED(g_sp->QueryService(IID_VDManagerPublic, IID_VDManagerPublic, &g_vdm))) g_vdm = nullptr;
  if (FAILED(g_sp->QueryService(IID_AppViewCollection, IID_AppViewCollection, &g_coll))) g_coll = nullptr;

  g_info.can_create = g_info.can_switch = g_info.can_remove = true;
  g_info.layout = wide(g_layout.revision);
  logf("desktop: ready on build %d, interface %s, layout [%s], %d desktop(s)",
       g_info.build, utf8(g_info.iid).c_str(), g_layout.revision, count_raw());
  return true;
}

const EngineInfo& engine_info() { return g_info; }

void shutdown() {
  if (g_vdm)  { ((IUnknown*)g_vdm)->Release();  g_vdm = nullptr; }
  if (g_coll) { ((IUnknown*)g_coll)->Release(); g_coll = nullptr; }
  if (g_mgr)  { ((IUnknown*)g_mgr)->Release();  g_mgr = nullptr; }
  if (g_sp)   { g_sp->Release();                g_sp = nullptr; }
  g_vt = nullptr;
  if (g_com) { CoUninitialize(); g_com = false; }
}

// Slots to try for an operation: the detected revision first, then every other
// revision's value. Wrong ones are rejected by their effect, so the exact
// mismatch pyvda warns about (same IID, shifted layout) still resolves.
static int candidates(int which, int* out) {
  int n = 0;
  int first = which == 0 ? g_layout.create_desktop
            : which == 1 ? g_layout.switch_desktop : g_layout.remove_desktop;
  if (first >= 0) out[n++] = first;
  for (int r = 0; r < REV_COUNT; r++) {
    int v = which == 0 ? kLayouts[r].create_desktop
          : which == 1 ? kLayouts[r].switch_desktop : kLayouts[r].remove_desktop;
    if (v < 0) continue;
    bool dup = false;
    for (int i = 0; i < n; i++) if (out[i] == v) { dup = true; break; }
    if (!dup) out[n++] = v;
  }
  return n;
}

static int count_raw() {
  if (!g_mgr) return -1;
  UINT n = 0;
  if (FAILED(guarded_vt1(g_mgr, g_layout.get_count, &n))) return -1;
  return (int)n;
}

int count() {
  if (!g_mgr) return 0;
  int n = count_raw();
  return n < 0 ? 0 : n;
}

static void collect(std::vector<DesktopInfo>* out) {
  out->clear();
  if (!g_mgr) return;
  IObjectArray* arr = nullptr;
  if (FAILED(guarded_vt1(g_mgr, g_layout.get_desktops, &arr)) || !arr) return;
  UINT n = 0;
  arr->GetCount(&n);
  for (UINT i = 0; i < n; i++) {
    IUnknown* d = nullptr;
    if (FAILED(arr->GetAt(i, *g_layout.desktop_iid, (void**)&d)) || !d) {
      if (FAILED(arr->GetAt(i, IID_IVirtualDesktopRev, (void**)&d)) || !d) {
        if (FAILED(arr->GetAt(i, IID_IVirtualDesktopOld, (void**)&d)) || !d) continue;
      }
    }
    GUID g = {};
    if (FAILED(guarded_vt1(d, kDesktopGetId, &g))) { d->Release(); continue; }
    wchar_t buf[40] = {};
    if (StringFromGUID2(g, (LPOLESTR)buf, 40) > 0) {
      DesktopInfo di;
      di.id = buf;
      di.index = (int)i + 1;
      out->push_back(di);
    }
    d->Release();
  }
  arr->Release();
}

std::vector<DesktopInfo> list() {
  std::vector<DesktopInfo> v;
  collect(&v);
  return v;
}

static bool current_guid(GUID* out) {
  if (!g_mgr) return false;
  void* d = nullptr;
  if (FAILED(guarded_vt1(g_mgr, g_layout.get_current, &d)) || !d) return false;
  bool ok = SUCCEEDED(guarded_vt1(d, kDesktopGetId, out));
  ((IUnknown*)d)->Release();
  return ok;
}

int current_index() {
  GUID cur = {};
  if (!current_guid(&cur)) return -1;
  for (auto& d : list()) {
    GUID g = {};
    if (CLSIDFromString(d.id.c_str(), &g) != S_OK) continue;
    if (memcmp(&g, &cur, sizeof(GUID)) == 0) return d.index;
  }
  return -1;
}

static void* desktop_object(int index) {
  if (!g_mgr) return nullptr;
  IObjectArray* arr = nullptr;
  if (FAILED(guarded_vt1(g_mgr, g_layout.get_desktops, &arr)) || !arr) return nullptr;
  UINT n = 0;
  arr->GetCount(&n);
  void* result = nullptr;
  if (index >= 1 && (UINT)index <= n) {
    IUnknown* d = nullptr;
    if (SUCCEEDED(arr->GetAt((UINT)(index - 1), *g_layout.desktop_iid, (void**)&d)) && d) result = d;
    else if (SUCCEEDED(arr->GetAt((UINT)(index - 1), IID_IVirtualDesktopRev, (void**)&d)) && d) result = d;
  }
  arr->Release();
  return result;
}

int create_desktop() {
  if (!g_mgr) return -1;
  int slots[8];
  int n = candidates(0, slots);
  int home = current_index();   // a wrong slot can switch desktops; restore it
  for (int i = 0; i < n; i++) {
    int before = count();
    void* d = nullptr;
    HRESULT hr = guarded_vt1(g_mgr, slots[i], &d);
    if (d) ((IUnknown*)d)->Release();
    int after = count();
    if (after <= before && home > 0 && current_index() != home) {
      logf("desktop: create slot %d switched the view; restoring desktop %d", slots[i], home);
      switch_desktop(home);
    }
    if (after > before) {
      g_layout.create_desktop = slots[i];
      logf("desktop: created (%d -> %d) via slot %d [%s]", before, after, slots[i], g_layout.revision);
      return after;
    }
    logf("desktop: create slot %d no effect (hr=0x%08lX, %d -> %d)", slots[i], (unsigned long)hr, before, after);
  }
  logf("desktop: no working CreateDesktopW slot on build %d", g_info.build);
  return -1;
}

bool remove_desktop(int index, int fallback) {
  if (index == fallback || !g_mgr) return false;
  int slots[8];
  int n = candidates(2, slots);
  for (int i = 0; i < n; i++) {
    int before = count();
    void* d = desktop_object(index);
    void* f = desktop_object(fallback);
    if (!d || !f) { if (d) ((IUnknown*)d)->Release(); if (f) ((IUnknown*)f)->Release(); return false; }
    HRESULT hr = guarded_vt2(g_mgr, slots[i], d, f);
    ((IUnknown*)d)->Release();
    ((IUnknown*)f)->Release();
    if (count() < before) {
      g_layout.remove_desktop = slots[i];
      return true;
    }
    logf("desktop: remove slot %d no effect (hr=0x%08lX)", slots[i], (unsigned long)hr);
  }
  return false;
}

bool switch_desktop(int index) {
  if (!g_mgr) return false;
  int slots[8];
  int n = candidates(1, slots);
  void* d = desktop_object(index);
  if (!d) return false;
  for (int i = 0; i < n; i++) {
    HRESULT hr = guarded_vt1(g_mgr, slots[i], d);
    if (current_index() == index) {
      g_layout.switch_desktop = slots[i];
      logf("desktop: switched to %d via slot %d [%s]", index, slots[i], g_layout.revision);
      ((IUnknown*)d)->Release();
      return true;
    }
    logf("desktop: switch slot %d no effect (hr=0x%08lX)", slots[i], (unsigned long)hr);
  }
  ((IUnknown*)d)->Release();
  logf("desktop: no working SwitchDesktop slot on build %d", g_info.build);
  return false;
}

bool move_window(HWND hwnd, int index) {
  if (!hwnd) return false;
  void* d = desktop_object(index);
  if (!d) return false;

  void* view = nullptr;
  if (g_coll)
    ((PFN_hwndout)(*(void***)g_coll)[kCollGetViewForHwnd])(g_coll, hwnd, &view);

  if (view) {
    HRESULT hr = guarded_vt2(g_mgr, g_layout.move_view, view, d);
    ((IUnknown*)view)->Release();
    ((IUnknown*)d)->Release();
    if (FAILED(hr)) logf("desktop: MoveViewToDesktop(%d) hr=0x%08lX", index, (unsigned long)hr);
    return SUCCEEDED(hr);
  }

  GUID g = {};
  bool ok = false;
  if (g_vdm) {
    for (auto& di : list())
      if (di.index == index) { CLSIDFromString(di.id.c_str(), &g); break; }
    ok = SUCCEEDED(((PFN_hwndmove)(*(void***)g_vdm)[kPubMove])(g_vdm, hwnd, &g));
  }
  ((IUnknown*)d)->Release();
  if (!ok) logf("desktop: no view for hwnd %p; window not moved", (void*)hwnd);
  return ok;
}

int window_desktop(HWND hwnd) {
  if (!hwnd || !g_mgr || !g_vdm) return -1;
  GUID g = {};
  if (SUCCEEDED(((PFN_hwndguid)(*(void***)g_vdm)[kPubGetId])(g_vdm, hwnd, &g))) {
    for (auto& d : list()) {
      GUID dg = {};
      if (CLSIDFromString(d.id.c_str(), &dg) == S_OK && memcmp(&dg, &g, sizeof(GUID)) == 0) return d.index;
    }
  }
  int on = 0;
  if (SUCCEEDED(((PFN_iscur)(*(void***)g_vdm)[kPubIsOnCurrent])(g_vdm, hwnd, &on)) && on) return current_index();
  return -1;
}

ResolvedDesktop resolve(const std::string& specRaw, bool allow_create) {
  ResolvedDesktop r;
  std::string spec = trim(specRaw);
  int total = count();
  if (total <= 0) { r.error = "no virtual desktop manager available"; return r; }

  if (spec.empty() || iequals(spec, "new")) {
    r.index = create_desktop();
    if (r.index <= 0) { r.error = "could not create a desktop"; return r; }
    r.created = true; r.ok = true;
    return r;
  }
  if (iequals(spec, "current")) {
    r.index = current_index();
    if (r.index <= 0) { r.error = "could not read the current desktop"; return r; }
    r.ok = true;
    return r;
  }

  auto grow_to = [&](int want) -> bool {
    if (want <= total) return true;
    if (!allow_create) { r.error = "desktop " + std::to_string(want) + " does not exist (create=false)"; return false; }
    while (total < want) {
      if (create_desktop() <= 0) { r.error = "failed creating desktop " + std::to_string(total + 1); return false; }
      int now = count();
      total = now > total ? now : total + 1;
      r.created = true;
    }
    return true;
  };

  if (spec[0] == '+' || spec[0] == '-') {
    char* end = nullptr;
    long delta = strtol(spec.c_str(), &end, 10);
    if (end == spec.c_str() || *end != '\0') { r.error = "invalid desktop spec '" + spec + "'"; return r; }
    int base = current_index();
    if (base <= 0) base = 1;
    int want = base + (int)delta;
    if (want < 1) want = 1;
    if (!grow_to(want)) return r;
    if (want > count()) { r.error = "desktop " + std::to_string(want) + " is out of range"; return r; }
    r.index = want; r.ok = true;
    return r;
  }

  char* end = nullptr;
  long n = strtol(spec.c_str(), &end, 10);
  if (end != spec.c_str() && *end == '\0') {
    int want = (int)n;
    if (want < 1) { r.error = "desktop index must be 1 or greater"; return r; }
    if (!grow_to(want)) return r;
    if (want > count()) { r.error = "desktop " + std::to_string(want) + " is out of range"; return r; }
    r.index = want; r.ok = true;
    return r;
  }

  for (auto& d : list())
    if (iequals(utf8(d.id), spec)) { r.index = d.index; r.ok = true; return r; }
  r.error = "no desktop matching '" + spec + "'";
  return r;
}

}  // namespace vd
