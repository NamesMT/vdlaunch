#pragma once
#include <windows.h>

namespace vd {

// MinGW has no __try/__except, and the internal desktop manager *faults* on a
// wrong vtable slot instead of returning an error. A vectored exception handler
// plus longjmp turns that into a recoverable failure so a wrong guess cannot
// kill the process.
void guard_install();

// Calls a vtable slot with the fault guard armed. A wrong slot returns
// E_UNEXPECTED instead of terminating the process.
HRESULT guarded_vt1(void* obj, int slot, void* a1);
HRESULT guarded_vt2(void* obj, int slot, void* a1, void* a2);

}  // namespace vd
