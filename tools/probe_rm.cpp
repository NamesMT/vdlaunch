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
typedef HRESULT(STDMETHODCALLTYPE*FO)(void*,void**);
typedef HRESULT(STDMETHODCALLTYPE*F45)(void*,void*,void*);
typedef HRESULT(STDMETHODCALLTYPE*FC)(void*,UINT*);
static void* mgr; static void** vt;
static UINT cnt(){UINT n=0;((FC)vt[3])(mgr,&n);return n;}
static void* dobj(int idx){IObjectArray*a=nullptr;if(FAILED(((F2)vt[7])(mgr,&a))||!a)return nullptr;IUnknown*d=nullptr;if(a->GetAt((UINT)idx-1,IID_IVD,(void**)&d)!=0)d=nullptr;a->Release();return d;}
static LONG CALLBACK veh(EXCEPTION_POINTERS*ep){printf("  FAULT 0x%08lX\n",(unsigned long)ep->ExceptionRecord->ExceptionCode);return EXCEPTION_EXECUTE_HANDLER;}
int main(int argc,char**argv){
  setvbuf(stdout,nullptr,_IONBF,0);
  int slot=argc>1?atoi(argv[1]):12;
  CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
  IServiceProvider* sp=nullptr; CoCreateInstance(CLSID_IS,nullptr,CLSCTX_LOCAL_SERVER,IID_SP,(void**)&sp);
  sp->QueryService(SID_VDMI,IID_VDMI,&mgr); vt=*(void***)mgr;
  UINT before=cnt();
  AddVectoredExceptionHandler(1,veh);
  void* kill=dobj((int)before); void* keep=dobj(1);
  printf("slot=%d before=%u kill_obj=%p keep_obj=%p\n",slot,before,(void*)kill,(void*)keep);
  HRESULT r=((F45)vt[slot])(mgr,kill,keep);
  printf(" hr=0x%08lX after=%u => %s\n",(unsigned long)r,cnt(),cnt()<before?"*** REMOVED ***":"no change");
  return 0;
}
