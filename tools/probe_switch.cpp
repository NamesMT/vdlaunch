#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <objbase.h>
#include <shobjidl.h>
#include <servprov.h>
#include <cstdio>
#include <cstring>
static const GUID CLSID_ImmersiveShell={0xC2F03A33,0x21F5,0x47FA,{0xB4,0xBB,0x15,0x63,0x62,0xA2,0xF2,0x39}};
static const GUID SID_VDMI={0xC5E0CDCA,0x7B6E,0x41B2,{0x9F,0xC4,0xD9,0x39,0x75,0xCC,0x46,0x7B}};
static const GUID IID_VDMI={0x53F5CA0B,0x158F,0x4124,{0x90,0x0C,0x05,0x71,0x58,0x06,0x0B,0x27}};
static const GUID IID_IVD={0x3F07F4BE,0xB107,0x441A,{0xAF,0x0F,0x39,0xD8,0x25,0x29,0x07,0x2C}};
static const GUID IID_SP={0x6D5140C1,0x7436,0x11CE,{0x80,0x34,0x00,0xAA,0x00,0x60,0x09,0xFA}};
static LONG CALLBACK veh(EXCEPTION_POINTERS* ep){printf("  FAULT 0x%08lX @%p\n",(unsigned long)ep->ExceptionRecord->ExceptionCode,ep->ExceptionRecord->ExceptionAddress);return EXCEPTION_EXECUTE_HANDLER;}
typedef HRESULT(STDMETHODCALLTYPE*F1)(void*,UINT*);
typedef HRESULT(STDMETHODCALLTYPE*F2)(void*,IObjectArray**);
typedef HRESULT(STDMETHODCALLTYPE*F3)(void*,GUID*);
typedef HRESULT(STDMETHODCALLTYPE*F4)(void*,void*);
typedef HRESULT(STDMETHODCALLTYPE*F5)(void*,void**,void**);
typedef HRESULT(STDMETHODCALLTYPE*FO)(void*,void**);
static void gs(const GUID&g,char*s){StringFromGUID2(g,(LPOLESTR)s,64);}
int main(int argc,char**argv){
  setvbuf(stdout,nullptr,_IONBF,0);
  DWORD mode = (argc>1 && !strcmp(argv[1],"mta")) ? COINIT_MULTITHREADED : COINIT_APARTMENTTHREADED;
  printf("mode=%s\n", mode==COINIT_MULTITHREADED?"MTA":"STA");
  if (mode==COINIT_MULTITHREADED) CoInitializeEx(nullptr, COINIT_MULTITHREADED);
  else CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
  IServiceProvider* sp=nullptr;
  HRESULT hr=CoCreateInstance(CLSID_ImmersiveShell,nullptr,CLSCTX_LOCAL_SERVER,IID_SP,(void**)&sp);
  printf("shell hr=0x%08lX sp=%p\n",(unsigned long)hr,(void*)sp);
  if(!sp) return 1;
  void* mgr=nullptr; hr=sp->QueryService(SID_VDMI,IID_VDMI,&mgr);
  printf("mgr hr=0x%08lX mgr=%p vt=%p\n",(unsigned long)hr,mgr,mgr?*(void***)mgr:0);
  if(!mgr) return 1;
  AddVectoredExceptionHandler(1,veh);
  void** vt=*(void***)mgr;
  // GetDesktops slot 7
  IObjectArray* arr=nullptr;
  hr=((F2)vt[7])(mgr,&arr);
  printf("GetDesktops(slot7) hr=0x%08lX arr=%p\n",(unsigned long)hr,(void*)arr);
  if(!arr) return 1;
  UINT n=0; arr->GetCount(&n); printf("count=%u\n",n);
  void* d[8]={};
  for(UINT i=0;i<n&&i<8;i++){ IUnknown* x=nullptr; if(SUCCEEDED(arr->GetAt(i,IID_IVD,(void**)&x))) d[i]=x; }
  GUID id[8]={};
  for(UINT i=0;i<n&&i<8;i++){ if(d[i]){ HRESULT r=((F3)(*(void***)d[i])[4])(d[i],&id[i]); char s[64]; gs(id[i],s); printf(" desktop[%u] id=%ls (getid hr=0x%08lX)\n",i,s,(unsigned long)r);} }
  // GetCurrentDesktop slot 6
  void* cur=nullptr; hr=((FO)vt[6])(mgr,&cur);
  GUID curid={}; if(cur){ ((F3)(*(void***)cur)[4])(cur,&curid); char s[64]; gs(curid,s); printf("current desktop id=%ls\n",s);} else printf("GetCurrentDesktop hr=0x%08lX (null)\n",(unsigned long)hr);
  // find index of current
  int curIdx=-1; for(UINT i=0;i<n&&i<8;i++) if(IsEqualGUID(id[i],curid)) curIdx=i;
  printf("current index=%d\n",curIdx);
  // try SwitchDesktop at slots 8,9,10 on target desktop (pick a different one)
  int tgt = (curIdx==0)?1:0;
  if(tgt>=(int)n || !d[tgt]){ printf("no alternate desktop to test\n"); return 0; }
  for(int slot=8; slot<=11; slot++){
    printf("-- try SwitchDesktop slot %d -> desktop[%d]\n",slot,tgt); fflush(stdout);
    void* cur2=nullptr; HRESULT g=((FO)vt[6])(mgr,&cur2); GUID before={}; if(cur2) ((F3)(*(void***)cur2)[4])(cur2,&before);
    HRESULT r=((F4)vt[slot])(mgr,d[tgt]);
    Sleep(700);
    cur2=nullptr; g=((FO)vt[6])(mgr,&cur2); GUID after={}; if(cur2) ((F3)(*(void***)cur2)[4])(cur2,&after);
    char sb[64],sa[64]; gs(before,sb); gs(after,sa);
    printf("   hr=0x%08lX before=%ls after=%ls => %s\n",(unsigned long)r,sb,sa, IsEqualGUID(after,id[tgt])?"SWITCHED OK":"no change");
    if (IsEqualGUID(after,id[tgt])) { printf("   ^^ WORKING SwitchDesktop slot = %d\n", slot); break; }
  }
  printf("DONE\n"); return 0;
}
