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
static const GUID IID_SP={0x6D5140C1,0x7436,0x11CE,{0x80,0x34,0x00,0xAA,0x00,0x60,0x09,0xFA}};
static const GUID IID_AVC={0x1841C6D7,0x4F9D,0x42C0,{0xAF,0x41,0x87,0x47,0x53,0x8F,0x10,0xE5}};
typedef HRESULT(STDMETHODCALLTYPE*F5)(void*,HWND,void**);
static LONG CALLBACK veh(EXCEPTION_POINTERS*ep){printf("  FAULT 0x%08lX\n",(unsigned long)ep->ExceptionRecord->ExceptionCode);return EXCEPTION_EXECUTE_HANDLER;}
int main(int argc,char**argv){
  setvbuf(stdout,nullptr,_IONBF,0);
  int slot=argc>1?atoi(argv[1]):6;
  CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
  WNDCLASSA wc={0}; wc.lpfnWndProc=DefWindowProcA; wc.hInstance=GetModuleHandleA(0); wc.lpszClassName="VdAvcWnd";
  RegisterClassA(&wc);
  HWND own=CreateWindowExA(0,"VdAvcWnd","avc",WS_OVERLAPPEDWINDOW|WS_VISIBLE,60,60,300,180,nullptr,nullptr,wc.hInstance,nullptr);
  MSG m; for(int i=0;i<10;i++){while(PeekMessageA(&m,0,0,0,PM_REMOVE)){TranslateMessage(&m);DispatchMessageA(&m);}Sleep(30);}
  IServiceProvider* sp=nullptr;
  CoCreateInstance(CLSID_ImmersiveShell,nullptr,CLSCTX_LOCAL_SERVER,IID_SP,(void**)&sp);
  void* coll=nullptr; HRESULT hr=sp->QueryService(IID_AVC,IID_AVC,&coll);
  printf("avc qs hr=0x%08lX coll=%p vt=%p\n",(unsigned long)hr,coll,coll?*(void***)coll:0);
  if(!coll) return 1;
  AddVectoredExceptionHandler(1,veh);
  void* v=nullptr;
  printf("calling slot %d GetViewForHwnd(hwnd=%p)...\n",slot,(void*)own);
  HRESULT r=((F5)(*(void***)coll)[slot])(coll,own,&v);
  printf(" hr=0x%08lX view=%p %s\n",(unsigned long)r,(void*)v,v?"*** NON-NULL VIEW ***":"");
  if(v){ void** vt=*(void***)v; GUID g={}; ((HRESULT(STDMETHODCALLTYPE*)(void*,GUID*))vt[4])(v,&g); char s[64]; StringFromGUID2(g,(LPOLESTR)s,64); printf(" view desktop id=%ls\n",s); }
  DestroyWindow(own);
  return 0;
}
