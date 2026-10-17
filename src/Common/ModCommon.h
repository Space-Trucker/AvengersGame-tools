#pragma once
#include <windows.h>
#include <stdio.h>
#include <share.h>
#include <stdint.h>
#include <string.h>

#ifndef MOD_NAME
#error "define MOD_NAME (e.g. L\"QuickStart\") before including ModCommon.h"
#endif

static const uint32_t PE_TIMESTAMP = 1680579845;

static uintptr_t g_base;
static wchar_t g_dir[MAX_PATH];

static inline void Log(const char* fmt, ...) {
    wchar_t path[MAX_PATH]; swprintf_s(path, L"%s%s.log", g_dir, MOD_NAME);
    FILE* f = _wfsopen(path, L"a", _SH_DENYNO);
    if (!f) return;
    SYSTEMTIME t; GetLocalTime(&t);
    fprintf(f, "%02d:%02d:%02d ", t.wHour, t.wMinute, t.wSecond);
    va_list a; va_start(a, fmt); vfprintf(f, fmt, a); va_end(a);
    fputc('\n', f); fclose(f);
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
    if (memcmp(p, with, n) == 0) return true;
    if (memcmp(p, expect, n) != 0) return false;
    DWORD old;
    if (!VirtualProtect((void*)q, 8, PAGE_EXECUTE_READWRITE, &old)) return false;
    LONG64 before = *q, after = before;
    memcpy((uint8_t*)&after + at, with, n);
    bool ok = InterlockedCompareExchange64(q, after, before) == before;
    VirtualProtect((void*)q, 8, old, &old);
    FlushInstructionCache(GetCurrentProcess(), (void*)q, 8);
    return ok;
}

static inline bool SetPointer(uintptr_t* slot, void* value) {
    DWORD old;
    if (!VirtualProtect(slot, 8, PAGE_READWRITE, &old)) return false;
    InterlockedExchangePointer((void**)slot, value);
    VirtualProtect(slot, 8, old, &old);
    return true;
}

static void ModMain();
static DWORD WINAPI ModThread(LPVOID) {
    if (!InitGameBase()) { Log("wrong game version (needs Steam 2.8.2) - not installed"); return 0; }
    ModMain();
    return 0;
}
extern "C" __declspec(dllexport) int AvengersModInit(void*) { return 1; }
BOOL WINAPI DllMain(HINSTANCE h, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(h);
        GetModuleFileNameW(h, g_dir, MAX_PATH);
        wchar_t* s = wcsrchr(g_dir, L'\\'); if (s) s[1] = 0;
        HANDLE t = CreateThread(nullptr, 0, ModThread, nullptr, 0, nullptr);
        if (t) CloseHandle(t);
    }
    return TRUE;
}
