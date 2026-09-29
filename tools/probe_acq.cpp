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
typedef HRESULT(STDMETHODCALLTYPE*F1)(void*,UINT*);
typedef HRESULT(STDMETHODCALLTYPE*F2)(void*,IObjectArray**);
typedef HRESULT(STDMETHODCALLTYPE*F3)(void*,GUID*);
typedef HRESULT(STDMETHODCALLTYPE*FQ)(void*,const GUID&,void**);
static void report(const char* tag, void* o){
  if(!o){printf("%s: null\n",tag);return;}
  void** vt=*(void***)o;
  UINT c=(UINT)-1;
  printf("%s obj=%p vt=%p ",tag,o,(void*)vt);
  HRESULT hr=((F1)vt[3])(o,&c);
  printf("slot3(GetCount?) hr=0x%08lX val=%d\n",(unsigned long)hr,c);
}
int main(){
  setvbuf(stdout,nullptr,_IONBF,0); AddVectoredExceptionHandler(1,veh);
  CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
  printf("HKLM IApplicationViewCollection IID check below\n");

  // path A: CoCreateInstance shell -> QueryService(CLSID as service)
  IServiceProvider* sp=nullptr;
  printf("A shell=0x%08lX\n",(unsigned long)CoCreateInstance(CLSID_ImmersiveShell,nullptr,CLSCTX_LOCAL_SERVER,IID_SP,(void**)&sp));
  void* a=nullptr; printf("A QS=0x%08lX\n",(unsigned long)sp->QueryService(SID_VDMI,IID_VDMI,&a)); report("A",a);

  // path B: QI on the returned pointer with a DIFFERENT iid, then back
  if(a){ void* b=nullptr; HRESULT hr=((IUnknown*)a)->QueryInterface(IID_IVD,&b); printf("B QI(IVD) hr=0x%08lX b=%p\n",(unsigned long)hr,b); report("B",b); }

  // path C: QI shell for the service id as IID
  { void* c=nullptr; HRESULT hr=((IUnknown*)sp)->QueryInterface(SID_VDMI,&c); printf("C QI(shell, SID as IID) hr=0x%08lX c=%p\n",(unsigned long)hr,c); report("C",c); }

  // path D: direct CoCreateInstance on internal CLSID
  { void* d=nullptr; HRESULT hr=CoCreateInstance(SID_VDMI,nullptr,CLSCTX_LOCAL_SERVER,IID_VDMI,&d); printf("D CoCreateInstance(SID) hr=0x%08lX d=%p\n",(unsigned long)hr,d); report("D",d); }
  { void* e=nullptr; HRESULT hr=CoCreateInstance(SID_VDMI,nullptr,CLSCTX_ALL,IID_VDMI,&e); printf("E CoCreateInstance(SID,ALL) hr=0x%08lX e=%p\n",(unsigned long)hr,e); report("E",e); }

  // path F: GetDesktops from A, then GetCount on desktop objects
  if(a){ IObjectArray* arr=nullptr; HRESULT hr=((F2)(*(void***)a)[7])(a,&arr); printf("F GetDesktops hr=0x%08lX arr=%p\n",(unsigned long)hr,(void*)arr);
    if(arr) for(UINT i=0;i<2;i++){ IUnknown* x=nullptr; HRESULT g=arr->GetAt(i,IID_IVD,(void**)&x); printf("  GetAt(%u)=0x%08lX x=%p\n",i,(unsigned long)g,(void*)x); report("  DESKTOP",x); } }
  printf("DONE\n"); return 0;
}
