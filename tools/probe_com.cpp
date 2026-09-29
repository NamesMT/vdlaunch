#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <objbase.h>
#include <shobjidl.h>
#include <cstdio>
#include <cstring>

static const GUID CLSID_ImmersiveShell = {0xC2F03A33,0x21F5,0x47FA,{0xB4,0xBB,0x15,0x63,0x62,0xA2,0xF2,0x39}};

#define HR(x) (printf("%-46s hr=0x%08lX %s\n", #x, (unsigned long)(x), (x)==0?"OK":""), (x))

int main() {
  HR(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED));

  // 1) Documented IVirtualDesktopManager via CoCreateInstance
  {
    void* p = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_ImmersiveShell, nullptr, CLSCTX_LOCAL_SERVER, IID_IVirtualDesktopManager, &p);
    printf("  CoCreateInstance(ImmersiveShell, IVirtualDesktopManager) -> hr=0x%08lX ptr=%p\n", (unsigned long)hr, p);
    if (SUCCEEDED(hr) && p) {
      IDispatch* pv = nullptr;
      hr = reinterpret_cast<IUnknown*>(p)->QueryInterface(IID_IVirtualDesktopManager, (void**)&pv);
      printf("  QI -> 0x%08lX\n", (unsigned long)hr);
    }
  }

  // 2) Get IServiceProvider from ImmersiveShell
  void* sp = nullptr;
  HRESULT hr = CoCreateInstance(CLSID_ImmersiveShell, nullptr, CLSCTX_LOCAL_SERVER, IID_IServiceProvider, &sp);
  printf("\nCoCreateInstance(ImmersiveShell, IServiceProvider) hr=0x%08lX sp=%p\n", (unsigned long)hr, sp);
  if (!sp) return 1;

  // 3) Derive internal IIDs from parameterized CLSIDs (24H2/26100 formula: iid = 0xC2F03A33_21F5_47FA_B4BB_156362A2_<u16 LE><bt><bd>ll)
  auto mkIid = [](unsigned short u16, unsigned char bt, unsigned char bd, GUID& g) {
    g = CLSID_ImmersiveShell;
    g.Data4[6] = (unsigned char)(u16 & 0xFF);
    g.Data4[7] = (unsigned char)(u16 >> 8);
    g.Data4[2] = bt;  // within Data4[0..3] = B4 BB 15 63
    g.Data4[3] = bd;
  };

  const char* names[] = {"IVirtualDesktopManagerInternal", "IApplicationViewCollection"};
  unsigned short u16s[] = {0, 0};
  unsigned char bts[] = {0x15, 0x15};
  unsigned char bds[] = {0x63, 0x63};
  for (int i = 0; i < 2; i++) {
    GUID iid; mkIid(u16s[i], bts[i], bds[i], iid);
    char s[64]; StringFromGUID2(iid, (LPOLESTR)s, 64);
    void* out = nullptr;
    IUnknown* prov = (IUnknown*)sp;
    // IServiceProvider::QueryService is vtable slot 3 (after QI/AddRef/Release)
    typedef HRESULT (STDMETHODCALLTYPE *PFNQS)(void*, const GUID&, const GUID&, void**);
    PFNQS qs = (PFNQS)((*(void***)prov)[3]);
    hr = qs(prov, iid, iid, &out);
    printf("\n%s\n  derived IID=%ls QueryService hr=0x%08lX out=%p\n", names[i], s, (unsigned long)hr, out);
  }
  return 0;
}
