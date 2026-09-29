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
static const GUID IID_AVC={0x1841C6D7,0x4F9D,0x42C0,{0xAF,0x41,0x87,0x47,0x53,0x8F,0x10,0xE5}};
typedef HRESULT(STDMETHODCALLTYPE*F1)(void*,UINT*);
typedef HRESULT(STDMETHODCALLTYPE*F2)(void*,IObjectArray**);
typedef HRESULT(STDMETHODCALLTYPE*F3)(void*,GUID*);
typedef HRESULT(STDMETHODCALLTYPE*FO)(void*,void**);
typedef HRESULT(STDMETHODCALLTYPE*F4)(void*,void*);
typedef HRESULT(STDMETHODCALLTYPE*F45)(void*,void*,void*);
typedef HRESULT(STDMETHODCALLTYPE*F5)(void*,HWND,void**);
static void* mgr=nullptr; static void** mvt=nullptr;
static LONG CALLBACK veh(EXCEPTION_POINTERS* ep){printf("  FAULT 0x%08lX @%p\n",(unsigned long)ep->ExceptionRecord->ExceptionCode,ep->ExceptionRecord->ExceptionAddress);return EXCEPTION_EXECUTE_HANDLER;}
static void gs(const GUID&g,char*s){StringFromGUID2(g,(LPOLESTR)s,64);}
static GUID curId(){ void* c=nullptr; GUID g={}; if(SUCCEEDED(((FO)mvt[6])(mgr,&c))&&c) ((F3)(*(void***)c)[4])(c,&g); return g; }
static BOOL CALLBACK findOwn(HWND h, LPARAM lp){ DWORD pid=0; GetWindowThreadProcessId(h,&pid); if(pid==GetCurrentProcessId()&&IsWindowVisible(h)){ *(HWND*)lp=h; return FALSE;} return TRUE; }

int main(int argc,char**argv){
  setvbuf(stdout,nullptr,_IONBF,0);
  const char* what = argc>1?argv[1]:"switch";
  int slot = argc>2?atoi(argv[2]):8;
  CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
  AllocConsole();
  SetConsoleTitleA("vdlaunchprobe");
  { HWND cw=GetConsoleWindow(); ShowWindow(cw,SW_SHOW); Sleep(300); }
  IServiceProvider* sp=nullptr;
  CoCreateInstance(CLSID_ImmersiveShell,nullptr,CLSCTX_LOCAL_SERVER,IID_SP,(void**)&sp);
  sp->QueryService(SID_VDMI,IID_VDMI,&mgr); mvt=*(void***)mgr;
  IObjectArray* arr=nullptr; ((F2)mvt[7])(mgr,&arr);
  UINT n=0; arr->GetCount(&n);
  void* d[8]={}; GUID id[8]={};
  for(UINT i=0;i<n&&i<8;i++){ IUnknown* x=nullptr; if(SUCCEEDED(arr->GetAt(i,IID_IVD,(void**)&x))){ d[i]=x; ((F3)(*(void***)x)[4])(x,&id[i]); } }
  GUID c0=curId();
  int idx=-1; for(UINT i=0;i<n;i++) if(!memcmp(&id[i],&c0,sizeof(GUID))) idx=i;
  printf("count=%u curIdx=%d\n",n,idx);
  AddVectoredExceptionHandler(1,veh);

  if(!strcmp(what,"switch")){
    int t=(idx==0)?1:0; if(t>=(int)n||!d[t]){printf("no target\n");return 0;}
    printf("SwitchDesktop slot %d -> desktop[%d]\n",slot,t);
    HRESULT r=((F4)mvt[slot])(mgr,d[t]); printf(" hr=0x%08lX\n",(unsigned long)r);
    Sleep(900);
    GUID c1=curId(); char s[64]; gs(c1,s);
    printf(" now=%ls => %s\n",s, !memcmp(&c1,&id[t],sizeof(GUID))?"*** WORKING SWITCH SLOT ***":"no change");
    return 0;
  }
  if(!strcmp(what,"move")){
    // bring up our own window and move it to another desktop
    HWND own=nullptr; EnumWindows(findOwn,(LPARAM)&own);
    printf("own hwnd=%p\n",(void*)own);
    if(!own){ printf("no own window\n"); return 0; }
    IUnknown* coll=nullptr; HRESULT qh=sp->QueryService(IID_AVC,IID_AVC,(void**)&coll);
    printf("avc hr=0x%08lX coll=%p\n",(unsigned long)qh,(void*)coll);
    if(!coll) return 0;
    void* view=nullptr;
    HRESULT vh=((F5)(*(void***)coll)[3])(coll,own,&view);
    printf("GetViewForHwnd hr=0x%08lX view=%p\n",(unsigned long)vh,(void*)view);
    if(!view) return 0;
    GUID vb={}; HRESULT gh=((F3)(*(void***)view)[4])(view,&vb);
    char s[64]; gs(vb,s); printf("view desktops BEFORE=%ls (hr=0x%08lX)\n",s,(unsigned long)gh);
    int t=(idx==0)?1:0;
    printf("MoveViewToDesktop slot %d -> desktop[%d]\n",slot,t);
    HRESULT r=((F45)mvt[slot])(mgr,view,d[t]);
    printf(" hr=0x%08lX\n",(unsigned long)r);
    Sleep(400);
    GUID va={}; ((F3)(*(void***)view)[4])(view,&va); gs(va,s);
    printf("view desktops AFTER =%ls => %s\n",s, !memcmp(&va,&id[t],sizeof(GUID))?"*** MOVED OK ***":"not moved");
    return 0;
  }
  printf("unknown\n"); return 2;
}
