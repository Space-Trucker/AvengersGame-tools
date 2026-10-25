#pragma once
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <share.h>
#include <stdint.h>
#include <string.h>
#include "PatchLock.h"

#ifndef MOD_NAME
#error "define MOD_NAME (e.g. L\"QuickStart\") before including ModCommon.h"
#endif

static const uint32_t PE_TIMESTAMP = 1680579845;

static uintptr_t g_base;
static wchar_t g_dir[MAX_PATH];

static inline void Log(const char* fmt, ...) {
    if (!g_dir[0]) return;
    wchar_t path[MAX_PATH];
    if (_snwprintf_s(path, _countof(path), _TRUNCATE, L"%s%s.log", g_dir, MOD_NAME) < 0) return;
    FILE* f = _wfsopen(path, L"a", _SH_DENYNO);
    if (!f) return;
    SYSTEMTIME t; GetLocalTime(&t);
    fprintf(f, "%02d:%02d:%02d ", t.wHour, t.wMinute, t.wSecond);
    va_list a; va_start(a, fmt); vfprintf(f, fmt, a); va_end(a);
    fputc('\n', f); fclose(f);
}

static void __cdecl OnInvalidParameter(const wchar_t*, const wchar_t*, const wchar_t*, unsigned, uintptr_t) {
    static volatile LONG n;
    if (InterlockedIncrement(&n) <= 20) Log("CRT: invalid parameter (a format or buffer bug) - ignored");
}

static inline bool InitGameBase() {
    g_base = (uintptr_t)GetModuleHandleW(nullptr);
    auto nt = (IMAGE_NT_HEADERS*)(g_base + ((IMAGE_DOS_HEADER*)g_base)->e_lfanew);
    return nt->FileHeader.TimeDateStamp == PE_TIMESTAMP;
}

static inline int FaultFilter(DWORD code) {
    return code == EXCEPTION_ACCESS_VIOLATION || code == EXCEPTION_IN_PAGE_ERROR ? EXCEPTION_EXECUTE_HANDLER
                                                                                  : EXCEPTION_CONTINUE_SEARCH;
}
#define FAULT __except (FaultFilter(GetExceptionCode()))

static inline bool PatchCode(uintptr_t rva, const void* expect, const void* with, size_t n) {
    uint8_t* p = (uint8_t*)(g_base + rva);
    volatile LONG64* q = (volatile LONG64*)((uintptr_t)p & ~(uintptr_t)7);
    size_t at = (size_t)(p - (uint8_t*)q);
    if (n == 0 || at + n > 8) return false;
    PatchLock lock;
    if (!lock.ok()) return false;
    LONG64 before = *q, after = before;
    if (memcmp((uint8_t*)&before + at, with, n) == 0) return true;
    if (memcmp((uint8_t*)&before + at, expect, n) != 0) return false;
    memcpy((uint8_t*)&after + at, with, n);
    DWORD old;
    if (!VirtualProtect((void*)q, 8, PAGE_EXECUTE_READWRITE, &old)) return false;
    bool ok = InterlockedCompareExchange64(q, after, before) == before;
    VirtualProtect((void*)q, 8, old, &old);
    FlushInstructionCache(GetCurrentProcess(), (void*)q, 8);
    return ok;
}

static inline bool SwapPointer(uintptr_t* slot, uintptr_t expected, uintptr_t desired) {
    if (((uintptr_t)slot & 7) != 0) return false;
    PatchLock lock;
    if (!lock.ok()) return false;
    DWORD old;
    if (!VirtualProtect(slot, 8, PAGE_READWRITE, &old)) return false;
    bool ok = (uintptr_t)InterlockedCompareExchangePointer((void* volatile*)slot, (void*)desired, (void*)expected) == expected;
    VirtualProtect(slot, 8, old, &old);
    return ok;
}

static inline bool SetPointer(uintptr_t* slot, void* value) {
    PatchLock lock;
    if (!lock.ok()) return false;
    DWORD old;
    if (!VirtualProtect(slot, 8, PAGE_READWRITE, &old)) return false;
    InterlockedExchangePointer((void**)slot, value);
    VirtualProtect(slot, 8, old, &old);
    return true;
}

static void ModMain();
static DWORD WINAPI ModThread(LPVOID) {
    _set_invalid_parameter_handler(OnInvalidParameter);
    if (!InitGameBase()) { Log("wrong game version (needs Steam 2.8.2) - not installed"); return 0; }
    ModMain();
    return 0;
}
extern "C" __declspec(dllexport) int AvengersModInit(void*) { return 1; }
BOOL WINAPI DllMain(HINSTANCE h, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DWORD n = GetModuleFileNameW(h, g_dir, MAX_PATH);
        wchar_t* s = n && n < MAX_PATH ? wcsrchr(g_dir, L'\\') : nullptr;
        if (s) s[1] = 0; else g_dir[0] = 0;
        HANDLE t = CreateThread(nullptr, 0, ModThread, nullptr, 0, nullptr);
        if (t) CloseHandle(t);
    }
    return TRUE;
}
