#include "guard.h"
#include <csetjmp>

namespace vd {

static jmp_buf       g_jmp;
static volatile int  g_armed = 0;
static volatile bool g_tripped = false;
static PVOID         g_veh = nullptr;

static LONG CALLBACK veh(EXCEPTION_POINTERS* ep) {
  DWORD code = ep->ExceptionRecord->ExceptionCode;
  if (g_armed && (code == EXCEPTION_ACCESS_VIOLATION || code == EXCEPTION_ILLEGAL_INSTRUCTION ||
                  code == EXCEPTION_PRIV_INSTRUCTION || code == EXCEPTION_IN_PAGE_ERROR)) {
    g_armed = 0;
    g_tripped = true;
    longjmp(g_jmp, 1);
  }
  return EXCEPTION_CONTINUE_SEARCH;
}

void guard_install() {
  if (!g_veh) g_veh = AddVectoredExceptionHandler(1, veh);
}

HRESULT guarded_vt1(void* obj, int slot, void* a1) {
  typedef HRESULT(STDMETHODCALLTYPE * FN)(void*, void*);
  FN fn = (FN)(*(void***)obj)[slot];
  g_tripped = false;
  if (setjmp(g_jmp) == 0) {
    g_armed = 1;
    HRESULT hr = fn(obj, a1);
    g_armed = 0;
    return hr;
  }
  return (HRESULT)0x8000FFFFL;  // E_UNEXPECTED: the call faulted
}

HRESULT guarded_vt2(void* obj, int slot, void* a1, void* a2) {
  typedef HRESULT(STDMETHODCALLTYPE * FN)(void*, void*, void*);
  FN fn = (FN)(*(void***)obj)[slot];
  g_tripped = false;
  if (setjmp(g_jmp) == 0) {
    g_armed = 1;
    HRESULT hr = fn(obj, a1, a2);
    g_armed = 0;
    return hr;
  }
  return (HRESULT)0x8000FFFFL;
}

}  // namespace vd
