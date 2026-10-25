#pragma once
#include <windows.h>

static const wchar_t* const PATCH_MUTEX = L"Local\\AvengersModSlotHooks";
static const DWORD PATCH_LOCK_WAIT_MS = 10000;

struct PatchLock {
    HANDLE h; bool owned;
    PatchLock() : h(CreateMutexW(nullptr, FALSE, PATCH_MUTEX)), owned(false) {
        if (h) { DWORD r = WaitForSingleObject(h, PATCH_LOCK_WAIT_MS); owned = r == WAIT_OBJECT_0 || r == WAIT_ABANDONED; }
    }
    ~PatchLock() { if (owned) ReleaseMutex(h); if (h) CloseHandle(h); }
    PatchLock(const PatchLock&) = delete;
    PatchLock& operator=(const PatchLock&) = delete;
    bool ok() const { return owned; }
};
