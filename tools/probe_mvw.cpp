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
typedef HRESULT(STDMETHODCALLTYPE*F2)(void*,IObjectArray**);
typedef HRESULT(STDMETHODCALLTYPE*F3)(void*,GUID*);
typedef HRESULT(STDMETHODCALLTYPE*FO)(void*,void**);
typedef HRESULT(STDMETHODCALLTYPE*F45)(void*,void*,void*);
typedef HRESULT(STDMETHODCALLTYPE*F5)(void*,HWND,void**);
static void* mgr; static void** mvt;
static void gs(const GUID&g,char*s){StringFromGUID2(g,(LPOLESTR)s,64);}
static IUnknown* g_vdm=nullptr;
static HRESULT vdmIsOnCur(HWND h){ typedef HRESULT(STDMETHODCALLTYPE*FI)(void*,HWND,int*); if(!g_vdm)return E_FAIL; int on=-1; HRESULT r=((FI)(*(void***)g_vdm)[3])(g_vdm,h,&on); return r==0?(on?S_OK:S_FALSE):r; }
static GUID curId(){void*c=nullptr;GUID g={};if(SUCCEEDED(((FO)mvt[6])(mgr,&c))&&c)((F3)(*(void***)c)[4])(c,&g);return g;}
int main(int argc,char**argv){
  setvbuf(stdout,nullptr,_IONBF,0);
  int slot=argc>1?atoi(argv[1]):4;
  CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
  WNDCLASSA wc={0}; wc.lpfnWndProc=DefWindowProcA; wc.hInstance=GetModuleHandleA(0); wc.lpszClassName="VdLaunchTestWnd";
  RegisterClassA(&wc);
  HWND own=CreateWindowExA(0,"VdLaunchTestWnd","vdlaunch probe",WS_OVERLAPPEDWINDOW|WS_VISIBLE,50,50,320,200,nullptr,nullptr,wc.hInstance,nullptr);
  MSG msg; for(int i=0;i<20;i++){ while(PeekMessageA(&msg,nullptr,0,0,PM_REMOVE)){TranslateMessage(&msg);DispatchMessageA(&msg);} Sleep(50);}
  printf("own hwnd=%p visible=%d\n",(void*)own,IsWindowVisible(own));
  IServiceProvider* sp=nullptr;
  CoCreateInstance(CLSID_ImmersiveShell,nullptr,CLSCTX_LOCAL_SERVER,IID_SP,(void**)&sp);
  sp->QueryService(SID_VDMI,IID_VDMI,&mgr); mvt=*(void***)mgr;
  { static const GUID IID_VDMPUB={0xa5cd92ff,0x29be,0x454c,{0x8d,0x04,0xd8,0x28,0x79,0xfb,0x3f,0x1b}};
    void* m=nullptr; HRESULT h2=sp->QueryService(IID_VDMPUB,IID_VDMPUB,&m); printf("cached IVirtualDesktopManager hr=0x%08lX ptr=%p\n",(unsigned long)h2,m); if(SUCCEEDED(h2)) g_vdm=(IUnknown*)m; }
  IObjectArray* arr=nullptr; ((F2)mvt[7])(mgr,&arr);
  UINT n=0; arr->GetCount(&n);
  void* d[8]={}; GUID id[8]={};
  for(UINT i=0;i<n&&i<8;i++){ IUnknown* x=nullptr; if(SUCCEEDED(arr->GetAt(i,IID_IVD,(void**)&x))){ d[i]=x; ((F3)(*(void***)x)[4])(x,&id[i]); } }
  GUID c0=curId(); int idx=-1; for(UINT i=0;i<n;i++) if(!memcmp(&id[i],&c0,16)) idx=i;
  printf("count=%u curIdx=%d\n",n,idx);
  int t=(idx==0)?1:0; if(t>=(int)n||!d[t]){printf("no target\n");return 0;}
  IUnknown* coll=nullptr; printf("avc=0x%08lX\n",(unsigned long)sp->QueryService(IID_AVC,IID_AVC,(void**)&coll));
  if(!coll){printf("no avc\n");return 0;}
  void* view=nullptr; HRESULT vh=((F5)(*(void***)coll)[6])(coll,own,&view);
  printf("GetViewForHwnd hr=0x%08lX view=%p\n",(unsigned long)vh,(void*)view);
  if(!view) return 0;
  HRESULT b0=vdmIsOnCur(own);
  HRESULT r=((F45)mvt[slot])(mgr,view,d[t]);
  Sleep(400);
  HRESULT a0=vdmIsOnCur(own);
  Sleep(400);
  HRESULT a1=vdmIsOnCur(own);
  printf("slot %d: MoveViewToDesktop hr=0x%08lX | isOnCurrent() after=%ld,%ld,%ld\n",
     slot,(unsigned long)r,(long)a0,(long)a1,(long)vdmIsOnCur(own));
  // restore: move back to original desktop
  void* back=(idx>=0&&idx<(int)n)?d[idx]:d[0];
  ((F45)mvt[slot])(mgr,view,back);
  Sleep(400);
  printf("        restored -> isOnCurrent()=%ld (expect 0 = back on original)\n",(long)vdmIsOnCur(own));
  DestroyWindow(own);
  printf("DONE\n");
  return 0;
}
