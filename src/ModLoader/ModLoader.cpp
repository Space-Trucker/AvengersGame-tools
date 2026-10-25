#define _CRT_SECURE_NO_WARNINGS

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>
#include <string.h>
#include "..\Common\PatchLock.h"

typedef uintptr_t (WINAPI *Fwd8)(uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t);

static FARPROC RealVersionProc(const char* name) {
    static HMODULE real;
    if (!real) {
        wchar_t path[MAX_PATH];
        UINT n = GetSystemDirectoryW(path, MAX_PATH);
        if (!n || n > MAX_PATH - 14) return nullptr;
        wcscat_s(path, L"\\version.dll");
        real = LoadLibraryW(path);
        if (!real) return nullptr;
    }
    return GetProcAddress(real, name);
}

#define FWD(name)                                                                                     \
    extern "C" uintptr_t WINAPI Fwd_##name(uintptr_t a, uintptr_t b, uintptr_t c, uintptr_t d,        \
                                           uintptr_t e, uintptr_t f, uintptr_t g, uintptr_t h) {      \
        static Fwd8 fn;                                                                                \
        if (!fn) fn = (Fwd8)RealVersionProc(#name);                                                   \
        if (!fn) { SetLastError(ERROR_PROC_NOT_FOUND); return 0; }                                    \
        return fn(a, b, c, d, e, f, g, h);                                                            \
    }                                                                                                 \
    __pragma(comment(linker, "/export:" #name "=Fwd_" #name))
FWD(GetFileVersionInfoA) FWD(GetFileVersionInfoByHandle) FWD(GetFileVersionInfoExA) FWD(GetFileVersionInfoExW)
FWD(GetFileVersionInfoSizeA) FWD(GetFileVersionInfoSizeExA) FWD(GetFileVersionInfoSizeExW) FWD(GetFileVersionInfoSizeW)
FWD(GetFileVersionInfoW) FWD(VerFindFileA) FWD(VerFindFileW) FWD(VerInstallFileA) FWD(VerInstallFileW)
FWD(VerLanguageNameA) FWD(VerLanguageNameW) FWD(VerQueryValueA) FWD(VerQueryValueW)

struct ModApi {
    uint32_t version;
    uint32_t size;
    uint8_t* gameBase;
    int (*MatchRva)(uint32_t rva, const uint8_t* bytes, uint32_t len);
    int (*PatchRva)(uint32_t rva, const uint8_t* expected, const uint8_t* replacement, uint32_t len);
    void* (*FindPattern)(const uint8_t* bytes, const char* mask);
};
static_assert(sizeof(ModApi) == 0x28, "ScriptHook API layout");
typedef int (*ModInitFn)(const ModApi*);

static wchar_t g_dir[MAX_PATH];
static FILE* g_log;
static uint8_t* g_base;

static void Log(const char* fmt, ...) {
    if (!g_log) return;
    SYSTEMTIME t; GetLocalTime(&t);
    _lock_file(g_log);
    fprintf(g_log, "%02d:%02d:%02d ", t.wHour, t.wMinute, t.wSecond);
    va_list a; va_start(a, fmt); vfprintf(g_log, fmt, a); va_end(a);
    fputc('\n', g_log); fflush(g_log);
    _unlock_file(g_log);
}

static bool InImage(uint32_t rva, uint32_t len) {
    auto nt = (IMAGE_NT_HEADERS*)(g_base + ((IMAGE_DOS_HEADER*)g_base)->e_lfanew);
    return len && len <= 0x100000 && (uint64_t)rva + len <= nt->OptionalHeader.SizeOfImage;
}

extern "C" __declspec(dllexport) uint32_t ASH_GetVersion() { return 0x10000; }

extern "C" __declspec(dllexport) int ASH_MatchRva(uint32_t rva, const uint8_t* bytes, uint32_t len) {
    if (!bytes || !g_base || !InImage(rva, len)) return 0;
    return memcmp(g_base + rva, bytes, len) == 0;
}

extern "C" __declspec(dllexport) int ASH_PatchRva(uint32_t rva, const uint8_t* expected, const uint8_t* replacement, uint32_t len) {
    if (!replacement || !g_base || !InImage(rva, len)) { Log("  PatchRva(0x%X, %u): bad arguments", rva, len); return 0; }
    PatchLock lock;
    if (!lock.ok()) { Log("  PatchRva(0x%X, %u): could not take the patch lock - not patched", rva, len); return 0; }
    uint8_t* p = g_base + rva;
    if (memcmp(p, replacement, len) == 0) { Log("  PatchRva(0x%X, %u): already applied", rva, len); return 1; }
    if (expected && memcmp(p, expected, len) != 0) { Log("  PatchRva(0x%X, %u): game bytes differ - not patched", rva, len); return 0; }

    DWORD prot[0x100000 / 0x1000 + 2];
    uintptr_t first = (uintptr_t)p & ~(uintptr_t)0xFFF, last = ((uintptr_t)p + len - 1) & ~(uintptr_t)0xFFF;
    int pages = (int)((last - first) / 0x1000) + 1;
    for (int i = 0; i < pages; i++) {
        MEMORY_BASIC_INFORMATION mbi;
        if (!VirtualQuery((void*)(first + i * 0x1000), &mbi, sizeof(mbi))) { Log("  PatchRva(0x%X): VirtualQuery failed", rva); return 0; }
        prot[i] = mbi.Protect;
    }
    DWORD old;
    if (!VirtualProtect(p, len, PAGE_EXECUTE_READWRITE, &old)) { Log("  PatchRva(0x%X): VirtualProtect failed", rva); return 0; }
    memcpy(p, replacement, len);
    for (int i = 0; i < pages; i++) VirtualProtect((void*)(first + i * 0x1000), 1, prot[i], &old);
    FlushInstructionCache(GetCurrentProcess(), p, len);
    Log("  PatchRva(0x%X, %u): patched", rva, len);
    return 1;
}

extern "C" __declspec(dllexport) void* ASH_FindPattern(const uint8_t* bytes, const char* mask) {
    if (!bytes || !mask || !g_base) return nullptr;
    size_t n = strlen(mask);
    if (!n) return nullptr;
    auto nt = (IMAGE_NT_HEADERS*)(g_base + ((IMAGE_DOS_HEADER*)g_base)->e_lfanew);
    auto sec = IMAGE_FIRST_SECTION(nt);
    for (WORD i = 0; i < nt->FileHeader.NumberOfSections; i++, sec++) {
        if (memcmp(sec->Name, ".text\0\0\0", 8) != 0) continue;
        uint8_t* p = g_base + sec->VirtualAddress;
        size_t size = sec->Misc.VirtualSize;
        if (n > size) return nullptr;
        for (size_t o = 0; o <= size - n; o++) {
            size_t k = 0;
            while (k < n && (mask[k] == '?' || p[o + k] == bytes[k])) k++;
            if (k == n) return p + o;
        }
        return nullptr;
    }
    return nullptr;
}

static ModApi g_api = { 0x10000, sizeof(ModApi), nullptr, ASH_MatchRva, ASH_PatchRva, ASH_FindPattern };

static DWORD WINAPI LoadMods(LPVOID) {
    Sleep(500);
    wchar_t path[MAX_PATH];
    if (swprintf(path, MAX_PATH, L"%smods\\ModLoader.log", g_dir) < 0) return 0;
    g_log = _wfopen(path, L"w");
    Log("ModLoader %s - loading mods\\*.asi", MOD_VERSION);
    swprintf(path, MAX_PATH, L"%smods\\*.asi", g_dir);
    WIN32_FIND_DATAW fd;
    HANDLE f = FindFirstFileW(path, &fd);
    int n = 0;
    if (f != INVALID_HANDLE_VALUE) {
        do {
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;

            const wchar_t* ext = wcsrchr(fd.cFileName, L'.');
            if (!ext || _wcsicmp(ext, L".asi") != 0) continue;
            if (swprintf(path, MAX_PATH, L"%smods\\%s", g_dir, fd.cFileName) < 0) { Log("%ls: path too long - skipped", fd.cFileName); continue; }
            HMODULE m = LoadLibraryW(path);
            if (!m) { Log("%ls: could not load (error %lu)", fd.cFileName, GetLastError()); continue; }
            n++;
            auto init = (ModInitFn)GetProcAddress(m, "AvengersModInit");
            if (!init) { Log("%ls: loaded (no AvengersModInit)", fd.cFileName); continue; }
            Log("%ls: loaded, calling AvengersModInit", fd.cFileName);
            int r = 0;
            __try { r = init(&g_api); }
            __except (EXCEPTION_EXECUTE_HANDLER) { Log("%ls: AvengersModInit crashed (0x%08lX)", fd.cFileName, GetExceptionCode()); continue; }
            Log("%ls: AvengersModInit returned %d", fd.cFileName, r);
        } while (FindNextFileW(f, &fd));
        FindClose(f);
    }
    Log("done: %d mod(s) loaded", n);
    return 0;
}

BOOL WINAPI DllMain(HINSTANCE, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        wchar_t exe[MAX_PATH];
        DWORD len = GetModuleFileNameW(nullptr, exe, MAX_PATH);
        if (!len || len >= MAX_PATH) return TRUE;
        const wchar_t* name = wcsrchr(exe, L'\\');
        if (!name || _wcsicmp(name + 1, L"Avengers.exe") != 0) return TRUE;
        wcscpy_s(g_dir, exe);
        g_dir[name + 1 - exe] = 0;
        g_base = (uint8_t*)GetModuleHandleW(nullptr);
        g_api.gameBase = g_base;

        HANDLE t = CreateThread(nullptr, 0, LoadMods, nullptr, 0, nullptr);
        if (t) CloseHandle(t);
    }
    return TRUE;
}
