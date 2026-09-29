#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <objbase.h>
#include <shobjidl.h>
#include <servprov.h>
#include <cstdio>
#include <cstring>
static const GUID CLSID_IS={0xC2F03A33,0x21F5,0x47FA,{0xB4,0xBB,0x15,0x63,0x62,0xA2,0xF2,0x39}};
static const GUID SID_VDMI={0xC5E0CDCA,0x7B6E,0x41B2,{0x9F,0xC4,0xD9,0x39,0x75,0xCC,0x46,0x7B}};
static const GUID IID_VDMI={0x53F5CA0B,0x158F,0x4124,{0x90,0x0C,0x05,0x71,0x58,0x06,0x0B,0x27}};
static const GUID IID_IVD={0x3F07F4BE,0xB107,0x441A,{0xAF,0x0F,0x39,0xD8,0x25,0x29,0x07,0x2C}};
static const GUID IID_SP={0x6D5140C1,0x7436,0x11CE,{0x80,0x34,0x00,0xAA,0x00,0x60,0x09,0xFA}};
static const GUID IID_AVC={0x1841C6D7,0x4F9D,0x42C0,{0xAF,0x41,0x87,0x47,0x53,0x8F,0x10,0xE5}};
static const GUID IID_VDMPUB={0xa5cd92ff,0x29be,0x454c,{0x8d,0x04,0xd8,0x28,0x79,0xfb,0x3f,0x1b}};
typedef HRESULT(STDMETHODCALLTYPE*F2)(void*,IObjectArray**);
typedef HRESULT(STDMETHODCALLTYPE*F3)(void*,GUID*);
typedef HRESULT(STDMETHODCALLTYPE*FO)(void*,void**);
typedef HRESULT(STDMETHODCALLTYPE*F45)(void*,void*,void*);
typedef HRESULT(STDMETHODCALLTYPE*F5)(void*,HWND,void**);
static void* mgr; static void** mvt; static IUnknown* vdm;
static GUID curId(){void*c=nullptr;GUID g={};if(SUCCEEDED(((FO)mvt[6])(mgr,&c))&&c)((F3)(*(void***)c)[4])(c,&g);return g;}
static int isOnCur(HWND h){typedef HRESULT(STDMETHODCALLTYPE*FI)(void*,HWND,int*);int on=-1;((FI)(*(void***)vdm)[3])(vdm,h,&on);return on;}
static LONG CALLBACK veh(EXCEPTION_POINTERS*ep){printf("  FAULT 0x%08lX @%p\n",(unsigned long)ep->ExceptionRecord->ExceptionCode,ep->ExceptionRecord->ExceptionAddress);fflush(stdout);return EXCEPTION_EXECUTE_HANDLER;}
static int curIndex(void* d[],GUID id[],UINT n){GUID c=curId();for(UINT i=0;i<n;i++)if(!memcmp(&id[i],&c,16))return i;return -1;}
int main(){
  setvbuf(stdout,nullptr,_IONBF,0);
  CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
  IServiceProvider* sp=nullptr; CoCreateInstance(CLSID_IS,nullptr,CLSCTX_LOCAL_SERVER,IID_SP,(void**)&sp);
  sp->QueryService(SID_VDMI,IID_VDMI,&mgr); mvt=*(void***)mgr;
  { void* m=nullptr; sp->QueryService(IID_VDMPUB,IID_VDMPUB,&m); vdm=(IUnknown*)m; }
  printf("mgr=%p mgr_vt=%p slot7=%p slot10=%p\n",mgr,(void*)mvt,(void*)mvt[7],(void*)mvt[10]);
  IObjectArray* a=nullptr; HRESULT gr=((F2)mvt[7])(mgr,&a); UINT n=0; if(a)a->GetCount(&n); printf("GetDesktops hr=0x%08lX arr=%p count=%u\n",(unsigned long)gr,(void*)a,n);
  void* d[16]={}; GUID id[16]={};
  for(UINT i=0;i<n&&i<16;i++){IUnknown*x=nullptr;if(SUCCEEDED(a->GetAt(i,IID_IVD,(void**)&x))){d[i]=x;((F3)(*(void***)x)[4])(x,&id[i]);}}
  AddVectoredExceptionHandler(1,veh);
  int ci=curIndex(d,id,n);
  printf("desktops=%u current=%d\n",n,ci);
  // create a NEW desktop in the background (slot 10), verify we stay put
  void* fresh=nullptr; HRESULT cr=((HRESULT(STDMETHODCALLTYPE*)(void*,void**))mvt[10])(mgr,&fresh);
  Sleep(700);
  GUID fg={}; if(fresh)((F3)(*(void***)fresh)[4])(fresh,&fg);
  int ci2=curIndex(d,id,n);
  printf("CreateDesktopW hr=0x%08lX newdesktop=%p  current_after_create=%d (expect %d = stayed)\n",(unsigned long)cr,(void*)fresh,ci2,ci);

  // Q1: launch notepad in the BACKGROUND while staying on current desktop
  STARTUPINFOW si={0}; si.cb=sizeof(si); PROCESS_INFORMATION pi={0};
  wchar_t cmd[]=L"notepad.exe";
  BOOL ok=CreateProcessW(nullptr,cmd,nullptr,nullptr,FALSE,0,nullptr,nullptr,&si,&pi);
  printf("launched notepad ok=%d pid=%lu\n",ok,ok?pi.dwProcessId:0);
  struct FindWnd { DWORD pid; HWND* out; };
  HWND nw=nullptr; int onAtLaunch=-1;
  for(int i=0;i<80;i++){ Sleep(100);
    // find notepad window by pid
    FindWnd s={pi.dwProcessId,&nw};
    EnumWindows([](HWND h,LPARAM lp)->BOOL{auto* s=(FindWnd*)lp;DWORD p=0;GetWindowThreadProcessId(h,&p);if(p==s->pid&&IsWindowVisible(h)&&GetWindow(h,GW_OWNER)==nullptr){*s->out=h;return FALSE;}return TRUE;},(LPARAM)&s);
    if(nw){ onAtLaunch=isOnCur(nw); printf("  notepad window=%p appeared after %dms, onCurrentDesktop=%d\n",(void*)nw,(i+1)*100,onAtLaunch); break; }
  }
  if(!nw){printf("  notepad window never appeared\n");return 0;}
  printf("RESULT_Q1: launched on CURRENT desktop without switching = %s\n", onAtLaunch==1?"YES (background launch works natively)":"NO (must move window explicitly)");
  // now move it to the freshly created desktop silently and confirm we stay put
  IUnknown* coll=nullptr; sp->QueryService(IID_AVC,IID_AVC,(void**)&coll);
  void* view=nullptr; ((F5)(*(void***)coll)[6])(coll,nw,&view);
  HRESULT mr=((F45)mvt[4])(mgr,view,fresh);
  Sleep(600);
  printf("MoveViewToDesktop->new desktop hr=0x%08lX  window onCurrentDesktop now=%d (expect 0)  we are still on desktop %d\n",
     (unsigned long)mr, isOnCur(nw), curIndex(d,id,n));
  printf("Q1_ANSWER: app runs on a desktop the user never visited = %s\n", isOnCur(nw)==0?"YES":"NO");
  return 0;
}
