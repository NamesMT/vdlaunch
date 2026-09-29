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
static const GUID IID_IVD = {0x3F07F4BE,0xB107,0x441A,{0xAF,0x0F,0x39,0xD8,0x25,0x29,0x07,0x2C}};
static const GUID IID_IServiceProvider_local = {0x6D5140C1,0x7436,0x11CE,{0x80,0x34,0x00,0xAA,0x00,0x60,0x09,0xFA}};

static void gs(const GUID& g, char* s){ StringFromGUID2(g,(LPOLESTR)s,64); }

static LONG (CALLBACK *g_veh)(EXCEPTION_POINTERS*);
static LONG CALLBACK veh(EXCEPTION_POINTERS* ep){
  printf("   <<FAULT code=0x%08lX addr=%p>>\n",(unsigned long)ep->ExceptionRecord->ExceptionCode,ep->ExceptionRecord->ExceptionAddress);
  fflush(stdout); return EXCEPTION_EXECUTE_HANDLER;
}
// guarded call of vtable slot idx taking one opaque pointer arg, returns HRESULT
static int g_crashed = 0;
static HRESULT guarded(void* obj, int idx, void* a1) {
  typedef HRESULT (STDMETHODCALLTYPE *F)(void*, void*);
  void** vt = *(void***)obj;
  F f = (F)vt[idx];
  return f(obj, a1);
}

int main(){
  setvbuf(stdout,nullptr,_IONBF,0);
  AddVectoredExceptionHandler(1, veh);
  HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
  printf("CoInitialize hr=0x%08lX\n",(unsigned long)hr);
  IServiceProvider* sp=nullptr;
  hr = CoCreateInstance(CLSID_ImmersiveShell,nullptr,CLSCTX_LOCAL_SERVER,IID_IServiceProvider_local,(void**)&sp);
  printf("ImmersiveShell hr=0x%08lX\n",(unsigned long)hr); if(!sp) return 1;
  void* mgr=nullptr;
  hr=sp->QueryService(SID_VDMInternal,IID_VDMInternal,&mgr);
  printf("VDMInternal hr=0x%08lX mgr=%p\n",(unsigned long)hr,mgr); if(!mgr) return 2;
  UINT cnt=0; printf("GetCount->0x%08lX\n",(unsigned long)((HRESULT(STDMETHODCALLTYPE*)(void*,UINT*))(*(void***)mgr)[3])(mgr,&cnt));
  printf("count=%u\n",cnt);
  IObjectArray* arr=nullptr;
  printf("GetDesktops->0x%08lX\n",(unsigned long)((HRESULT(STDMETHODCALLTYPE*)(void*,IObjectArray**))(*(void***)mgr)[7])(mgr,&arr));
  printf("arr=%p\n",(void*)arr); if(!arr) return 3;
  for(UINT i=0;i<2;i++){
    IUnknown* d=nullptr;
    HRESULT gh=arr->GetAt(i,IID_IVD,(void**)&d);
    printf("GetAt(%u) hr=0x%08lX d=%p\n",i,(unsigned long)gh,(void*)d);
    if(FAILED(gh)||!d) continue;
    void** vt=*(void***)d;
    printf("  vt=%p 3=%p 4=%p 5=%p 6=%p\n",(void*)vt,vt[3],vt[4],vt[5],vt[6]);
    for(int slot=3; slot<=6; slot++){
      GUID g={}; g.Data1=0xDEADBEEF;
      printf("  calling slot %d ...\n",slot); fflush(stdout);
      HRESULT r2;
      if (slot==3) { typedef HRESULT(STDMETHODCALLTYPE*F)(void*,GUID*); r2=((F)vt[slot])(d,&g); }
      else if (slot==4) { typedef HRESULT(STDMETHODCALLTYPE*F)(void*,GUID*); r2=((F)vt[slot])(d,&g); }
      else { typedef HRESULT(STDMETHODCALLTYPE*F)(void*,void*); r2=((F)vt[slot])(d,&g); }
      char s[64]; gs(g,s);
      printf("   slot %d hr=0x%08lX guid=%ls\n",slot,(unsigned long)r2,s); fflush(stdout);
    }
    break;
  }
  printf("DONE\n");
  return 0;
}
