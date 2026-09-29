#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <objbase.h>
#include <shobjidl.h>
#include <servprov.h>
#include <cstdio>
#include <cstdlib>
static const GUID CLSID_ImmersiveShell={0xC2F03A33,0x21F5,0x47FA,{0xB4,0xBB,0x15,0x63,0x62,0xA2,0xF2,0x39}};
static const GUID SID_VDMI={0xC5E0CDCA,0x7B6E,0x41B2,{0x9F,0xC4,0xD9,0x39,0x75,0xCC,0x46,0x7B}};
static const GUID IID_VDMI={0x53F5CA0B,0x158F,0x4124,{0x90,0x0C,0x05,0x71,0x58,0x06,0x0B,0x27}};
static const GUID IID_IVD={0x3F07F4BE,0xB107,0x441A,{0xAF,0x0F,0x39,0xD8,0x25,0x29,0x07,0x2C}};
static const GUID IID_SP={0x6D5140C1,0x7436,0x11CE,{0x80,0x34,0x00,0xAA,0x00,0x60,0x09,0xFA}};
typedef HRESULT(STDMETHODCALLTYPE*F1)(void*,UINT*);
typedef HRESULT(STDMETHODCALLTYPE*F2)(void*,IObjectArray**);
typedef HRESULT(STDMETHODCALLTYPE*F3)(void*,GUID*);
typedef HRESULT(STDMETHODCALLTYPE*FQ)(void*);
static void* mgr=nullptr;
static char g_mode[32];
static int g_slot=0;
static void* get(const char* what, void* mgr_, IObjectArray* arr){
  if(!strcmp(what,"mgr")) return mgr_;
  IUnknown* d=nullptr; arr->GetAt(0,IID_IVD,(void**)&d); return d;
}
static LONG CALLBACK veh(EXCEPTION_POINTERS* ep){
  printf("  FAULT 0x%08lX @%p\n",(unsigned long)ep->ExceptionRecord->ExceptionCode,ep->ExceptionRecord->ExceptionAddress);
  fflush(stdout); return EXCEPTION_EXECUTE_HANDLER;
}
int main(int argc,char**argv){
  setvbuf(stdout,nullptr,_IONBF,0);
  if(argc<3){printf("usage: probe_isolate <mgr|vtd> <slot>\n");return 2;}
  strncpy(g_mode,argv[1],31); g_slot=atoi(argv[2]);
  CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
  IServiceProvider* sp=nullptr;
  if(FAILED(CoCreateInstance(CLSID_ImmersiveShell,nullptr,CLSCTX_LOCAL_SERVER,IID_SP,(void**)&sp))){printf("shell fail\n");return 3;}
  if(FAILED(sp->QueryService(SID_VDMI,IID_VDMI,&mgr))){printf("qs fail\n");return 3;}
  IObjectArray* arr=nullptr;
  if(FAILED(((F2)(*(void***)mgr)[7])(mgr,&arr))){printf("desktops fail\n");return 3;}
  void* o=get(g_mode,mgr,arr);
  if(!o){printf("obj null\n");return 3;}
  void** vt=*(void***)o;
  printf("mode=%s slot=%d obj=%p vt=%p fn=%p\n",g_mode,g_slot,o,(void*)vt,vt[g_slot]); fflush(stdout);
  AddVectoredExceptionHandler(1,veh);
  // unique marker so we can tell if the callee wrote anything
  unsigned char buf[64]; memset(buf,0xAB,sizeof(buf));
  HRESULT hr;
  switch(g_slot%4){
    case 0: hr=((F1)vt[g_slot])(o,(UINT*)buf); break;
    case 1: hr=((F2)vt[g_slot])(o,(IObjectArray**)buf); break;
    case 2: hr=((F3)vt[g_slot])(o,(GUID*)buf); break;
    default: hr=((FQ)vt[g_slot])(o); break;
  }
  printf("  returned hr=0x%08lX buf:",(unsigned long)hr);
  for(int i=0;i<24;i++) printf(" %02X",buf[i]);
  printf("\n"); fflush(stdout);
  return 0;
}
