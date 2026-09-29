#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <objbase.h>
#include <shobjidl.h>
#include <servprov.h>
#include <cstdio>
static const GUID CLSID_IS={0xC2F03A33,0x21F5,0x47FA,{0xB4,0xBB,0x15,0x63,0x62,0xA2,0xF2,0x39}};
static const GUID SID_VDMI={0xC5E0CDCA,0x7B6E,0x41B2,{0x9F,0xC4,0xD9,0x39,0x75,0xCC,0x46,0x7B}};
static const GUID IID_VDMI={0x53F5CA0B,0x158F,0x4124,{0x90,0x0C,0x05,0x71,0x58,0x06,0x0B,0x27}};
static const GUID IID_IVD={0x3F07F4BE,0xB107,0x441A,{0xAF,0x0F,0x39,0xD8,0x25,0x29,0x07,0x2C}};
static const GUID IID_SP={0x6D5140C1,0x7436,0x11CE,{0x80,0x34,0x00,0xAA,0x00,0x60,0x09,0xFA}};
typedef HRESULT(STDMETHODCALLTYPE*F2)(void*,IObjectArray**);
typedef HRESULT(STDMETHODCALLTYPE*F3)(void*,GUID*);
typedef HRESULT(STDMETHODCALLTYPE*FO)(void*,void**);
int main(){
  setvbuf(stdout,nullptr,_IONBF,0);
  CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
  IServiceProvider* sp=nullptr; CoCreateInstance(CLSID_IS,nullptr,CLSCTX_LOCAL_SERVER,IID_SP,(void**)&sp);
  void* mgr=nullptr; sp->QueryService(SID_VDMI,IID_VDMI,&mgr); void** vt=*(void***)mgr;
  IObjectArray* a=nullptr; ((F2)vt[7])(mgr,&a); UINT n=0;a->GetCount(&n);
  printf("desktop count=%u\n",n);
  for(UINT i=0;i<n;i++){IUnknown*x=nullptr;if(SUCCEEDED(a->GetAt(i,IID_IVD,(void**)&x))){GUID g={};((F3)(*(void***)x)[4])(x,&g);char s[64];StringFromGUID2(g,(LPOLESTR)s,64);printf(" [%u] %ls\n",i,s);}}
  void* c=nullptr; ((FO)vt[6])(mgr,&c); GUID cg={}; if(c)((F3)(*(void***)c)[4])(c,&cg);
  char s[64];StringFromGUID2(cg,(LPOLESTR)s,64);printf("current=%ls\n",s);
  return 0;
}
