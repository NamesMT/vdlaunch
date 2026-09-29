#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <psapi.h>
#include <objbase.h>
#include <shobjidl.h>
#include <servprov.h>
#include <cstdio>
static const GUID CLSID_ImmersiveShell={0xC2F03A33,0x21F5,0x47FA,{0xB4,0xBB,0x15,0x63,0x62,0xA2,0xF2,0x39}};
static const GUID SID_VDMI={0xC5E0CDCA,0x7B6E,0x41B2,{0x9F,0xC4,0xD9,0x39,0x75,0xCC,0x46,0x7B}};
static const GUID IID_VDMI={0x53F5CA0B,0x158F,0x4124,{0x90,0x0C,0x05,0x71,0x58,0x06,0x0B,0x27}};
static const GUID IID_IVD={0x3F07F4BE,0xB107,0x441A,{0xAF,0x0F,0x39,0xD8,0x25,0x29,0x07,0x2C}};
static const GUID IID_SP={0x6D5140C1,0x7436,0x11CE,{0x80,0x34,0x00,0xAA,0x00,0x60,0x09,0xFA}};
static void whichmod(const char* tag, void* addr){
  HMODULE mods[512]; DWORD need=0;
  if(!EnumProcessModules(GetCurrentProcess(),mods,sizeof(mods),&need)){printf("%s %p: EnumProcessModules failed\n",tag,addr);return;}
  int n=need/sizeof(HMODULE);
  for(int i=0;i<n;i++){ MODULEINFO mi={}; GetModuleInformation(GetCurrentProcess(),mods[i],&mi,sizeof(mi));
    if(addr>=(void*)mi.lpBaseOfDll && addr<(void*)((char*)mi.lpBaseOfDll+mi.SizeOfImage)){
      char nm[MAX_PATH]={0}; GetModuleFileNameA(mods[i],nm,MAX_PATH);
      printf("%s %p is INSIDE %s (base=%p size=0x%lX off=0x%llX)\n",tag,addr,nm,mi.lpBaseOfDll,(unsigned long)mi.SizeOfImage,(unsigned long long)((char*)addr-(char*)mi.lpBaseOfDll));
      return; } }
  printf("%s %p NOT inside any loaded module\n",tag,addr);
}
int main(){
  setvbuf(stdout,nullptr,_IONBF,0);
  CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
  IServiceProvider* sp=nullptr;
  printf("shell=0x%08lX\n",(unsigned long)CoCreateInstance(CLSID_ImmersiveShell,nullptr,CLSCTX_LOCAL_SERVER,IID_SP,(void**)&sp));
  void* mgr=nullptr; printf("vdmi=0x%08lX\n",(unsigned long)sp->QueryService(SID_VDMI,IID_VDMI,&mgr));
  printf("mgr=%p vt=%p\n",mgr,*(void***)mgr); whichmod("manager-vtable",*(void***)mgr);
  void* m2=nullptr; printf("vdmi-again=0x%08lX m2=%p\n",(unsigned long)sp->QueryService(SID_VDMI,IID_VDMI,&m2),m2);
  printf("m2 vt=%p\n",*(void***)m2); whichmod("m2-vtable",*(void***)m2);
  IObjectArray* arr=nullptr;
  printf("arr=0x%08lX\n",(unsigned long)((HRESULT(STDMETHODCALLTYPE*)(void*,IObjectArray**))(*(void***)mgr)[7])(mgr,&arr));
  printf("arr=%p\n",(void*)arr);
  IUnknown* d=nullptr; printf("getat=0x%08lX\n",(unsigned long)arr->GetAt(0,IID_IVD,(void**)&d));
  printf("d=%p vt=%p\n",(void*)d,*(void***)d); whichmod("desktop-vtable",*(void***)d);
  printf("DONE\n");
  return 0;
}
