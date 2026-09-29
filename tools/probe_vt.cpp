#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <objbase.h>
#include <shobjidl.h>
#include <servprov.h>
#include <cstdio>
static const GUID CLSID_ImmersiveShell={0xC2F03A33,0x21F5,0x47FA,{0xB4,0xBB,0x15,0x63,0x62,0xA2,0xF2,0x39}};
static const GUID SID_VDMI={0xC5E0CDCA,0x7B6E,0x41B2,{0x9F,0xC4,0xD9,0x39,0x75,0xCC,0x46,0x7B}};
static const GUID IID_VDMI={0x53F5CA0B,0x158F,0x4124,{0x90,0x0C,0x05,0x71,0x58,0x06,0x0B,0x27}};
static const GUID IID_IVD={0x3F07F4BE,0xB107,0x441A,{0xAF,0x0F,0x39,0xD8,0x25,0x29,0x07,0x2C}};
static const GUID IID_SP={0x6D5140C1,0x7436,0x11CE,{0x80,0x34,0x00,0xAA,0x00,0x60,0x09,0xFA}};
static LONG CALLBACK veh(EXCEPTION_POINTERS* ep){printf("  <<FAULT 0x%08lX @%p>>\n",(unsigned long)ep->ExceptionRecord->ExceptionCode,ep->ExceptionRecord->ExceptionAddress);return EXCEPTION_EXECUTE_HANDLER;}
static void dump(const char* tag, void* o){
  printf("%s obj=%p\n",tag,o); void** vt=*(void***)o; printf("  vtable=%p\n",(void*)vt);
  __int64* q=(__int64*)vt;
  for(int i=-2;i<10;i++){ printf("   slot[%2d] = %p\n", i, (void*)(uintptr_t)q[i]); }
}
int main(){
  setvbuf(stdout,nullptr,_IONBF,0); AddVectoredExceptionHandler(1,veh);
  CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
  IServiceProvider* sp=nullptr;
  CoCreateInstance(CLSID_ImmersiveShell,nullptr,CLSCTX_LOCAL_SERVER,IID_SP,(void**)&sp);
  void* mgr=nullptr; sp->QueryService(SID_VDMI,IID_VDMI,&mgr);
  dump("MANAGER",mgr);
  IObjectArray* arr=nullptr;
  ((HRESULT(STDMETHODCALLTYPE*)(void*,IObjectArray**))(*(void***)mgr)[7])(mgr,&arr);
  IUnknown* d=nullptr; arr->GetAt(0,IID_IVD,(void**)&d);
  dump("DESKTOP",d);
  IUnknown* d2=nullptr; HRESULT hr=((IUnknown*)d)->QueryInterface(IID_IVD,(void**)&d2);
  printf("desktop QI(IVD) hr=0x%08lX d2=%p\n",(unsigned long)hr,(void*)d2);
  if(d2) dump("DESKTOP-2",d2);
  // try GetID at slot 3 with proper stdcall thunk
  typedef HRESULT(STDMETHODCALLTYPE*F)(void*,GUID*);
  GUID g={}; g.Data1=0xDEADBEEF;
  printf("call slot3 on desktop...\n");
  HRESULT r=((F)(*(void***)d)[3])(d,&g);
  char s[64]; StringFromGUID2(g,(LPOLESTR)s,64);
  printf("slot3 hr=0x%08lX guid=%ls\n",(unsigned long)r,s);
  printf("DONE\n"); return 0;
}
