#include "desktop.h"
#include "util.h"
#include <objbase.h>
#include <servprov.h>
#include <shobjidl.h>
#include <cstring>

namespace vd {

// Verified against Windows 11 build 26100/26200, where 24H2 shifted the
// internal vtable. Slot numbers are 0-based indices *including* IUnknown's
// three entries, as reached through the shell's COM proxy.
static const GUID CLSID_ImmersiveShell     = {0xC2F03A33,0x21F5,0x47FA,{0xB4,0xBB,0x15,0x63,0x62,0xA2,0xF2,0x39}};
static const GUID SID_VDManagerInternal    = {0xC5E0CDCA,0x7B6E,0x41B2,{0x9F,0xC4,0xD9,0x39,0x75,0xCC,0x46,0x7B}};
// Candidate IVirtualDesktopManagerInternal IIDs. Windows uses a different one
// per interface revision, so we probe newest-first rather than trusting a build
// number (per pyvda, 26100's IID can answer on 22631 too).
struct IidCandidate { const GUID* iid; const char* label; };
static const GUID IID_VDMI_26100 = {0x53F5CA0B,0x158F,0x4124,{0x90,0x0C,0x05,0x71,0x58,0x06,0x0B,0x27}};
static const GUID IID_VDMI_22631 = {0x4970BA3D,0xFD4E,0x4647,{0xBE,0xA3,0xD8,0x90,0x76,0xEF,0x4B,0x9C}};
static const GUID IID_VDMI_22621 = {0xA3175F2D,0x239C,0x4BD2,{0x8A,0xA0,0xEE,0xBA,0x8B,0x0B,0x13,0x8E}};
static const GUID IID_VDMI_21313 = {0xB2F925B9,0x5A0F,0x4D2E,{0x9F,0x4D,0x2B,0x15,0x07,0x59,0x3C,0x10}};
static const GUID IID_VDMI_20231 = {0x094AFE11,0x44F2,0x4BA0,{0x97,0x6F,0x29,0xA9,0x7E,0x26,0x3E,0xE0}};
static const GUID IID_VDMI_9000  = {0xF31574D6,0xB682,0x4CDC,{0xBD,0x56,0x18,0x27,0x86,0x0A,0xBE,0xC6}};
static const IidCandidate kIids[] = {
  {&IID_VDMI_26100, "26100+"}, {&IID_VDMI_22631, "22631"},
  {&IID_VDMI_22621, "22621"},  {&IID_VDMI_21313, "21313"},
  {&IID_VDMI_20231, "20231"},  {&IID_VDMI_9000,  "legacy"},
};
static const GUID IID_IVirtualDesktop26100 = {0x3F07F4BE,0xB107,0x441A,{0xAF,0x0F,0x39,0xD8,0x25,0x29,0x07,0x2C}};
static const GUID IID_AppViewCollection    = {0x1841C6D7,0x4F9D,0x42C0,{0xAF,0x41,0x87,0x47,0x53,0x8F,0x10,0xE5}};
static const GUID IID_VDManagerPublic      = {0xA5CD92FF,0x29BE,0x454C,{0x8D,0x04,0xD8,0x28,0x79,0xFB,0x3F,0x1B}};
static const GUID IID_IServiceProvider_    = {0x6D5140C1,0x7436,0x11CE,{0x80,0x34,0x00,0xAA,0x00,0x60,0x09,0xFA}};

struct Layout {
  int get_count = 3;
  int move_view = 4;
  int get_current = 6;
  int get_desktops = 7;
  // Candidate vtable slots per interface revision, tried in order. The switch
  // and remove slots moved between revisions, and a wrong slot faults or
  // invokes the wrong method, so neither is trusted from a build number alone.
  int         switch_candidates[2] = {-1, -1};
  int         create_candidates[2] = {-1, -1};
  int         remove_candidates[2] = {-1, -1};
  const char* revision = "unverified";
};

enum : int {
  kGetCount          = 3,
  kMoveViewToDesktop = 4,
  kGetCurrentDesktop = 6,
  kGetDesktops       = 7,
  kDesktopGetId      = 4,
  kCollGetViewForHwnd = 6,
  kPublicIsWindowOnCurrent  = 3,
  kPublicGetWindowDesktopId = 4,
  kPublicMoveWindowToDesktop = 5,
};

typedef HRESULT(STDMETHODCALLTYPE* PFN_out1)(void*, void**);
typedef HRESULT(STDMETHODCALLTYPE* PFN_hwndout)(void*, HWND, void**);
typedef HRESULT(STDMETHODCALLTYPE* PFN_outarr)(void*, IObjectArray**);
typedef HRESULT(STDMETHODCALLTYPE* PFN_guidout)(void*, GUID*);
typedef HRESULT(STDMETHODCALLTYPE* PFN_1arg)(void*, void*);
typedef HRESULT(STDMETHODCALLTYPE* PFN_2arg)(void*, void*, void*);
typedef HRESULT(STDMETHODCALLTYPE* PFN_count)(void*, UINT*);
typedef HRESULT(STDMETHODCALLTYPE* PFN_iscur)(void*, HWND, int*);
typedef HRESULT(STDMETHODCALLTYPE* PFN_hwndguid)(void*, HWND, GUID*);
typedef HRESULT(STDMETHODCALLTYPE* PFN_hwndmove)(void*, HWND, GUID*);

static Layout            g_layout;
static bool              g_allow_unverified_slots = false;
static EngineInfo        g_info;
static IServiceProvider* g_sp   = nullptr;
static void*             g_mgr  = nullptr;
static void**            g_vt   = nullptr;
static void*             g_vdm  = nullptr;
static void*             g_coll = nullptr;
static bool              g_tried = false;
static bool              g_com   = false;

static int  count_raw();
static bool desktop_guid(int index, GUID* out);
static void* desktop_object(int index);

bool available() { return init(); }

bool init() {
  if (g_tried) return g_mgr != nullptr;
  g_tried = true;
  g_info.build = windows_build();

  HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
  if (hr == S_OK || hr == S_FALSE) g_com = true;
  else if (hr == RPC_E_CHANGED_MODE) g_com = false;
  else { logf("desktop: CoInitializeEx hr=0x%08lX", (unsigned long)hr); return false; }

  if (FAILED(CoCreateInstance(CLSID_ImmersiveShell, nullptr, CLSCTX_LOCAL_SERVER, IID_IServiceProvider_, (void**)&g_sp)) || !g_sp) {
    log_line("desktop: ImmersiveShell unavailable");
    g_sp = nullptr;
    return false;
  }

  // Probe newest first: the newest IID can also answer on an older build.
  for (auto& c : kIids) {
    void* m = nullptr;
    hr = g_sp->QueryService(SID_VDManagerInternal, *c.iid, &m);
    if (SUCCEEDED(hr) && m) {
      g_mgr = m;
      g_info.iid = wide(c.label);
      break;
    }
  }
  if (!g_mgr) {
    logf("desktop: no known IVirtualDesktopManagerInternal interface on build %d", g_info.build);
    return false;
  }
  g_vt = *(void***)g_mgr;

  // A wrong slot faults instead of failing, so prove the layout before use.
  int n = count_raw();
  if (n < 0) {
    log_line("desktop: manager layout probe failed; disabling virtual-desktop support");
    ((IUnknown*)g_mgr)->Release();
    g_mgr = nullptr;
    g_vt = nullptr;
    return false;
  }

  // Only the 26100/26200 revision has been verified slot-by-slot here. Older
  // revisions get the operations proven stable across builds, nothing more:
  // calling an unverified slot risks faulting or invoking the wrong method.
  // Verified slot-by-slot here for 26100/26200 only. Older revisions get the
  // candidate slots their interface published, and the mutating operations are
  // confirmed by effect before their result is trusted.
  g_layout = Layout{};
  if (g_info.iid == L"26100+") {
    // Verified slot-by-slot on builds 26100/26200.
    g_layout.switch_candidates[0] = 9;  g_layout.switch_candidates[1] = 8;
    g_layout.create_candidates[0] = 11; g_layout.create_candidates[1] = 10;
    g_layout.remove_candidates[0] = 13; g_layout.remove_candidates[1] = 12;
    g_layout.revision = "26100+ (verified)";
  } else if (g_info.iid == L"22631" || g_info.iid == L"22621") {
    g_layout.switch_candidates[0] = 8;  g_layout.switch_candidates[1] = 9;
    g_layout.create_candidates[0] = 9;  g_layout.create_candidates[1] = 10;
    g_layout.remove_candidates[0] = 10; g_layout.remove_candidates[1] = 11;
    g_layout.revision = "22621/22631 (candidates, unverified)";
  } else if (g_info.iid == L"21313" || g_info.iid == L"20231" || g_info.iid == L"legacy") {
    g_layout.switch_candidates[0] = 8;
    g_layout.create_candidates[0] = 9;
    g_layout.remove_candidates[0] = 10;
    g_layout.revision = "pre-22621 (candidates, unverified)";
  }

  // Slot tables for unverified revisions come from reference implementations and
  // have never run here. A wrong slot faults rather than failing softly, so the
  // mutating operations stay off unless the user opts in.
  bool verified = g_info.iid == L"26100+";
  bool allow_unverified = g_allow_unverified_slots;
  if (verified || allow_unverified) {
    g_info.can_create = g_info.can_switch = g_info.can_remove =
        g_layout.create_candidates[0] >= 0;
  }
  if (!verified && allow_unverified)
    logf("desktop: using unverified slot table [%s]; set experimental_layout=false if launches crash",
         g_layout.revision);

  if (FAILED(g_sp->QueryService(IID_VDManagerPublic, IID_VDManagerPublic, &g_vdm))) g_vdm = nullptr;
  if (FAILED(g_sp->QueryService(IID_AppViewCollection, IID_AppViewCollection, &g_coll))) g_coll = nullptr;

  g_info.layout = wide(g_layout.revision);
  logf("desktop: ready on build %d, interface %s, layout [%s], %d desktop(s)%s",
       g_info.build, utf8(g_info.iid).c_str(), g_layout.revision, n,
       g_info.can_create ? "" : " (create/switch unavailable on this build)");
  return true;
}

const EngineInfo& engine_info() { return g_info; }

void set_allow_unverified_slots(bool on) { g_allow_unverified_slots = on; }

void shutdown() {
  if (g_vdm)  { ((IUnknown*)g_vdm)->Release();  g_vdm = nullptr; }
  if (g_coll) { ((IUnknown*)g_coll)->Release(); g_coll = nullptr; }
  if (g_mgr)  { ((IUnknown*)g_mgr)->Release();  g_mgr = nullptr; }
  if (g_sp)   { g_sp->Release();                g_sp = nullptr; }
  g_vt = nullptr;
  if (g_com) { CoUninitialize(); g_com = false; }
}

static int count_raw() {
  if (!g_mgr) return -1;
  UINT n = 0;
  if (FAILED(((PFN_count)g_vt[kGetCount])(g_mgr, &n))) return -1;
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
  if (FAILED(((PFN_outarr)g_vt[kGetDesktops])(g_mgr, &arr)) || !arr) return;
  UINT n = 0;
  arr->GetCount(&n);
  for (UINT i = 0; i < n; i++) {
    IUnknown* d = nullptr;
    if (FAILED(arr->GetAt(i, IID_IVirtualDesktop26100, (void**)&d)) || !d) continue;
    GUID g = {};
    ((PFN_guidout)(*(void***)d)[kDesktopGetId])(d, &g);
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
  if (FAILED(((PFN_out1)g_vt[kGetCurrentDesktop])(g_mgr, &d)) || !d) return false;
  ((PFN_guidout)(*(void***)d)[kDesktopGetId])(d, out);
  ((IUnknown*)d)->Release();
  return true;
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

static bool desktop_guid(int index, GUID* out) {
  for (auto& d : list()) {
    if (d.index != index) continue;
    return CLSIDFromString(d.id.c_str(), out) == S_OK;
  }
  return false;
}

static void* desktop_object(int index) {
  if (!g_mgr) return nullptr;
  IObjectArray* arr = nullptr;
  if (FAILED(((PFN_outarr)g_vt[kGetDesktops])(g_mgr, &arr)) || !arr) return nullptr;
  UINT n = 0;
  arr->GetCount(&n);
  void* result = nullptr;
  if (index >= 1 && (UINT)index <= n) {
    IUnknown* d = nullptr;
    if (SUCCEEDED(arr->GetAt((UINT)(index - 1), IID_IVirtualDesktop26100, (void**)&d))) result = d;
  }
  arr->Release();
  return result;
}

int create_desktop() {
  if (!g_mgr) return -1;
  int before = count();
  for (int slot : g_layout.create_candidates) {
    if (slot < 0) continue;
    void* d = nullptr;
    HRESULT hr = ((PFN_out1)g_vt[slot])(g_mgr, &d);
    if (d) ((IUnknown*)d)->Release();
    int after = count();
    if (after > before) {
      logf("desktop: created desktop (%d -> %d) via slot %d [%s]", before, after, slot, g_layout.revision);
      return after;
    }
    if (FAILED(hr)) logf("desktop: CreateDesktopW candidate slot %d hr=0x%08lX", slot, (unsigned long)hr);
  }
  logf("desktop: no working CreateDesktopW slot on build %d (interface %s)", g_info.build, utf8(g_info.iid).c_str());
  return -1;
}

bool remove_desktop(int index, int fallback) {
  if (index == fallback) return false;
  if (!g_mgr) return false;
  int before = count();
  for (int slot : g_layout.remove_candidates) {
    if (slot < 0) continue;
    void* d = desktop_object(index);
    void* f = desktop_object(fallback);
    if (!d || !f) {
      if (d) ((IUnknown*)d)->Release();
      if (f) ((IUnknown*)f)->Release();
      return false;
    }
    HRESULT hr = ((PFN_2arg)g_vt[slot])(g_mgr, d, f);
    ((IUnknown*)d)->Release();
    ((IUnknown*)f)->Release();
    if (count() < before) {
      logf("desktop: removed desktop %d via slot %d [%s]", index, slot, g_layout.revision);
      return true;
    }
    if (FAILED(hr)) logf("desktop: RemoveDesktop candidate slot %d hr=0x%08lX", slot, (unsigned long)hr);
  }
  return false;
}

bool switch_desktop(int index) {
  if (!g_mgr) return false;
  void* d = desktop_object(index);
  if (!d) return false;

  int before = current_index();
  for (int slot : g_layout.switch_candidates) {
    if (slot < 0) continue;
    HRESULT hr = ((PFN_1arg)g_vt[slot])(g_mgr, d);
    if (current_index() == index) {
      logf("desktop: switched to desktop %d via slot %d [%s]", index, slot, g_layout.revision);
      ((IUnknown*)d)->Release();
      return true;
    }
    if (FAILED(hr)) logf("desktop: SwitchDesktop candidate slot %d hr=0x%08lX", slot, (unsigned long)hr);
    (void)before;
  }
  ((IUnknown*)d)->Release();
  logf("desktop: no working SwitchDesktop slot on build %d (interface %s)", g_info.build, utf8(g_info.iid).c_str());
  return false;
}

bool move_window(HWND hwnd, int index) {
  if (!hwnd) return false;
  void* d = desktop_object(index);
  if (!d) return false;

  void* view = nullptr;
  if (g_coll) {
    ((PFN_hwndout)(*(void***)g_coll)[kCollGetViewForHwnd])(g_coll, hwnd, &view);
  }
  if (view) {
    HRESULT hr = ((PFN_2arg)g_vt[kMoveViewToDesktop])(g_mgr, view, d);
    ((IUnknown*)view)->Release();
    ((IUnknown*)d)->Release();
    if (FAILED(hr)) logf("desktop: MoveViewToDesktop(%d) hr=0x%08lX", index, (unsigned long)hr);
    return SUCCEEDED(hr);
  }

  // No application view: the documented manager only moves our own windows.
  GUID g = {};
  bool ok = g_vdm && desktop_guid(index, &g) &&
            SUCCEEDED(((PFN_hwndmove)(*(void***)g_vdm)[kPublicMoveWindowToDesktop])(g_vdm, hwnd, &g));
  ((IUnknown*)d)->Release();
  if (!ok) logf("desktop: no view for hwnd %p; window not moved", (void*)hwnd);
  return ok;
}

int window_desktop(HWND hwnd) {
  if (!hwnd || !g_mgr) return -1;
  if (g_vdm) {
    GUID g = {};
    if (SUCCEEDED(((PFN_hwndguid)(*(void***)g_vdm)[kPublicGetWindowDesktopId])(g_vdm, hwnd, &g))) {
      for (auto& d : list()) {
        GUID dg = {};
        if (CLSIDFromString(d.id.c_str(), &dg) == S_OK && memcmp(&dg, &g, sizeof(GUID)) == 0) return d.index;
      }
    }
    int on = 0;
    if (SUCCEEDED(((PFN_iscur)(*(void***)g_vdm)[kPublicIsWindowOnCurrent])(g_vdm, hwnd, &on)) && on)
      return current_index();
  }
  return -1;
}

ResolvedDesktop resolve(const std::string& specRaw, bool allow_create) {
  ResolvedDesktop r;
  std::string spec = trim(specRaw);
  int total = count();
  if (total <= 0) {
    r.error = "no virtual desktop manager available";
    return r;
  }

  if (spec.empty() || iequals(spec, "new")) {
    r.index = create_desktop();
    if (r.index <= 0) { r.error = "could not create a desktop"; return r; }
    r.created = true;
    r.ok = true;
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
    if (!allow_create) {
      r.error = "desktop " + std::to_string(want) + " does not exist (create=false)";
      return false;
    }
    while (total < want) {
      if (create_desktop() <= 0) {
        r.error = "failed creating desktop " + std::to_string(total + 1);
        return false;
      }
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
    r.index = want;
    r.ok = true;
    return r;
  }

  char* end = nullptr;
  long n = strtol(spec.c_str(), &end, 10);
  if (end != spec.c_str() && *end == '\0') {
    int want = (int)n;
    if (want < 1) { r.error = "desktop index must be 1 or greater"; return r; }
    if (!grow_to(want)) return r;
    if (want > count()) { r.error = "desktop " + std::to_string(want) + " is out of range"; return r; }
    r.index = want;
    r.ok = true;
    return r;
  }

  for (auto& d : list()) {
    if (iequals(utf8(d.id), spec)) { r.index = d.index; r.ok = true; return r; }
  }
  r.error = "no desktop matching '" + spec + "'";
  return r;
}

}  // namespace vd
