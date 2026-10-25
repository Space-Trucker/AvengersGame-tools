#define MOD_VERSION "test"
#include "../src/ModLoader/ModLoader.cpp"
#include "Check.h"
#include "FakeImage.h"

static const uintptr_t TEXT_RVA = 0x10000, TEXT_SIZE = 0x10000, IMAGE_SIZE = 0x40000;
static wchar_t g_tmp[MAX_PATH];

static void LayHeader(FakeImage& im) {
    static uint8_t h[0x400];
    memset(h, 0, sizeof h);
    auto dos = (IMAGE_DOS_HEADER*)h;
    dos->e_magic = IMAGE_DOS_SIGNATURE;
    dos->e_lfanew = 0x80;
    auto nt = (IMAGE_NT_HEADERS64*)(h + 0x80);
    nt->Signature = IMAGE_NT_SIGNATURE;
    nt->FileHeader.Machine = IMAGE_FILE_MACHINE_AMD64;
    nt->FileHeader.NumberOfSections = 2;
    nt->FileHeader.SizeOfOptionalHeader = sizeof(IMAGE_OPTIONAL_HEADER64);
    nt->OptionalHeader.Magic = IMAGE_NT_OPTIONAL_HDR64_MAGIC;
    nt->OptionalHeader.SizeOfImage = (DWORD)IMAGE_SIZE;
    IMAGE_SECTION_HEADER* sec = IMAGE_FIRST_SECTION(nt);
    memcpy(sec[0].Name, ".rdata", 6);
    sec[0].VirtualAddress = 0x30000;
    sec[0].Misc.VirtualSize = 0x1000;
    memcpy(sec[1].Name, ".text", 5);
    sec[1].VirtualAddress = (DWORD)TEXT_RVA;
    sec[1].Misc.VirtualSize = (DWORD)TEXT_SIZE;
    CHECK(im.Put(0, h, sizeof h, PAGE_READONLY));
}

static FILE* OpenLog(const wchar_t* name) {
    wchar_t path[MAX_PATH];
    _snwprintf_s(path, _countof(path), _TRUNCATE, L"%s%s", g_tmp, name);
    return _wfopen(path, L"w");
}

static const char* ReadAll(const wchar_t* name) {
    static char text[1 << 22];
    text[0] = 0;
    wchar_t path[MAX_PATH];
    _snwprintf_s(path, _countof(path), _TRUNCATE, L"%s%s", g_tmp, name);
    FILE* f = nullptr;
    if (_wfopen_s(&f, path, L"rb") || !f) return text;
    size_t got = fread(text, 1, sizeof text - 1, f);
    text[got] = 0;
    fclose(f);
    return text;
}

static void TestApi() {
    FakeImage im(IMAGE_SIZE);
    LayHeader(im);
    CHECK(im.Commit(TEXT_RVA));
    DWORD old;
    VirtualProtect(im.At(TEXT_RVA), 0x1000, PAGE_EXECUTE_READ, &old);
    VirtualProtect(im.At(TEXT_RVA + 0x1000), 0x1000, PAGE_READONLY, &old);
    g_base = im.base;
    g_log = OpenLog(L"api.log");
    static const uint8_t cc[4] = { 0xCC, 0xCC, 0xCC, 0xCC }, a[4] = { 1, 2, 3, 4 }, b[4] = { 5, 6, 7, 8 };
    const uint32_t edge = (uint32_t)TEXT_RVA + 0xFFE;
    CHECK_EQ(ASH_GetVersion(), 0x10000);
    CHECK_EQ(ASH_MatchRva(edge, cc, 4), 1);
    CHECK_EQ(ASH_MatchRva(edge, a, 4), 0);
    CHECK_EQ(ASH_MatchRva((uint32_t)IMAGE_SIZE - 2, cc, 4), 0);
    CHECK_EQ(ASH_PatchRva(edge, cc, a, 4), 1);
    CHECK(memcmp(im.At(edge), a, 4) == 0);
    CHECK_EQ(im.Rights(TEXT_RVA), PAGE_EXECUTE_READ);
    CHECK_EQ(im.Rights(TEXT_RVA + 0x1000), PAGE_READONLY);
    CHECK_EQ(ASH_PatchRva(edge, cc, a, 4), 1);
    CHECK_EQ(ASH_PatchRva(edge, cc, b, 4), 0);
    CHECK(memcmp(im.At(edge), a, 4) == 0);
    CHECK_EQ(ASH_PatchRva(edge, nullptr, b, 4), 1);
    CHECK(memcmp(im.At(edge), b, 4) == 0);
    CHECK_EQ(ASH_PatchRva(edge, nullptr, nullptr, 4), 0);
    CHECK_EQ(ASH_PatchRva(edge, nullptr, a, 0), 0);
    CHECK_EQ(ASH_PatchRva((uint32_t)IMAGE_SIZE - 2, nullptr, a, 4), 0);
    static uint8_t big[0x100001];
    CHECK_EQ(ASH_PatchRva(0x100, nullptr, big, sizeof big), 0);

    static const uint8_t pattern[4] = { 0xDE, 0xAD, 0x00, 0xEF }, other[4] = { 0xDE, 0xAD, 0x77, 0xEF };
    CHECK(im.Put(TEXT_RVA + 0xF800, other, 4, PAGE_EXECUTE_READ));
    CHECK(im.Put(TEXT_RVA + 0xF900, pattern, 4, PAGE_EXECUTE_READ));
    CHECK(im.Commit(0x30000));
    CHECK(im.Put(0x30000, pattern, 4, PAGE_READONLY));
    CHECK(ASH_FindPattern(pattern, "xx?x") == im.At(TEXT_RVA + 0xF800));
    CHECK(ASH_FindPattern(pattern, "xxxx") == im.At(TEXT_RVA + 0xF900));
    static const uint8_t none[4] = { 0x0F, 0x1E, 0x2D, 0x3C };
    CHECK(ASH_FindPattern(none, "xxxx") == nullptr);
    CHECK(ASH_FindPattern(pattern, "") == nullptr);
    CHECK(ASH_FindPattern(nullptr, "xxxx") == nullptr);

    CHECK_EQ(g_api.version, 0x10000);
    CHECK_EQ(g_api.size, 0x28);
    CHECK(g_api.MatchRva == ASH_MatchRva && g_api.PatchRva == ASH_PatchRva && g_api.FindPattern == ASH_FindPattern);
    fclose(g_log);
    g_log = nullptr;
    const char* log = ReadAll(L"api.log");
    CHECK(strstr(log, "already applied") != nullptr);
    CHECK(strstr(log, "game bytes differ - not patched") != nullptr);
    CHECK(strstr(log, "bad arguments") != nullptr);
}

static const int PAGES = 10, CROSSING = PAGES - 1, THREADS = CROSSING + PAGES, FLIPS = 400;
static const DWORD PAGE_RIGHTS[PAGES] = { PAGE_EXECUTE_READ, PAGE_READONLY, PAGE_EXECUTE_READ, PAGE_READWRITE, PAGE_EXECUTE_READ,
                                          PAGE_READONLY, PAGE_EXECUTE_READ, PAGE_READWRITE, PAGE_EXECUTE_READ, PAGE_READONLY };
static HANDLE g_go;

struct PatchJob { uint32_t rva; uint8_t a[16], b[16]; int failed, faults; };

static DWORD WINAPI PatchLoop(LPVOID p) {
    PatchJob* job = (PatchJob*)p;
    WaitForSingleObject(g_go, INFINITE);
    for (int i = 0; i < FLIPS; i++) {
        __try {
            job->failed += ASH_PatchRva(job->rva, job->a, job->b, 16) != 1;
            job->failed += ASH_PatchRva(job->rva, job->b, job->a, 16) != 1;
        } __except (GetExceptionCode() == EXCEPTION_ACCESS_VIOLATION ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH) {
            job->faults++;
        }
    }
    return 0;
}

static void TestPatchRace() {
    FakeImage im(IMAGE_SIZE);
    LayHeader(im);
    CHECK(im.Commit(TEXT_RVA));
    g_base = im.base;
    g_log = OpenLog(L"race.log");
    static PatchJob jobs[THREADS];
    for (int t = 0; t < THREADS; t++) {
        PatchJob& j = jobs[t];
        j.rva = t < CROSSING ? (uint32_t)(TEXT_RVA + (t + 1) * 0x1000 - 8) : (uint32_t)(TEXT_RVA + (t - CROSSING) * 0x1000 + 0x800);
        for (int k = 0; k < 16; k++) { j.a[k] = (uint8_t)(0x10 + t); j.b[k] = (uint8_t)(0x80 + t); }
        j.failed = j.faults = 0;
        memcpy(im.At(j.rva), j.a, 16);
    }
    DWORD old;
    for (int k = 0; k < PAGES; k++) VirtualProtect(im.At(TEXT_RVA + k * 0x1000), 0x1000, PAGE_RIGHTS[k], &old);
    g_go = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    HANDLE h[THREADS];
    for (int t = 0; t < THREADS; t++) h[t] = CreateThread(nullptr, 0, PatchLoop, &jobs[t], 0, nullptr);
    SetEvent(g_go);
    WaitForMultipleObjects(THREADS, h, TRUE, INFINITE);
    int failed = 0, faults = 0, wrong = 0;
    for (int t = 0; t < THREADS; t++) {
        CloseHandle(h[t]);
        failed += jobs[t].failed;
        faults += jobs[t].faults;
        wrong += memcmp(im.At(jobs[t].rva), jobs[t].a, 16) != 0;
    }
    CloseHandle(g_go);
    CHECK_EQ(faults, 0);
    CHECK_EQ(failed, 0);
    CHECK_EQ(wrong, 0);
    for (int k = 0; k < PAGES; k++) CHECK_EQ(im.Rights(TEXT_RVA + k * 0x1000), PAGE_RIGHTS[k]);
    fclose(g_log);
    g_log = nullptr;

    const char* log = ReadAll(L"race.log");
    int lines = 0, bad = 0;
    for (const char* s = log; *s;) {
        const char* e = strchr(s, '\n');
        size_t n = e ? (size_t)(e - s) : strlen(s), length = n;
        if (length && s[length - 1] == '\r') length--;
        unsigned hh = 0, mm = 0, ss = 0, rva = 0, len = 0;
        char tail[32] = "";
        char line[128] = "";
        if (length < sizeof line) memcpy(line, s, length);
        bool ok = length < sizeof line && sscanf_s(line, "%2u:%2u:%2u   PatchRva(0x%X, %u): %31[a-z]", &hh, &mm, &ss, &rva, &len, tail,
                                              (unsigned)sizeof tail) == 6 && len == 16 && strcmp(tail, "patched") == 0;
        bad += !ok;
        lines++;
        s = e ? e + 1 : s + n;
    }
    CHECK_EQ(lines, THREADS * FLIPS * 2);
    CHECK_EQ(bad, 0);
}

static DWORD WINAPI LogLoop(LPVOID p) {
    int t = (int)(intptr_t)p;
    WaitForSingleObject(g_go, INFINITE);
    for (int i = 0; i < 500; i++) Log("thread %02d line %03d %s", t, i, "abcdefghijklmnopqrstuvwxyz0123456789abcdefghijklmnopqrstuvwxyz0123456789");
    return 0;
}

static void TestLogLines() {
    g_log = OpenLog(L"lines.log");
    const int threads = 8;
    g_go = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    HANDLE h[threads];
    for (int t = 0; t < threads; t++) h[t] = CreateThread(nullptr, 0, LogLoop, (LPVOID)(intptr_t)t, 0, nullptr);
    SetEvent(g_go);
    WaitForMultipleObjects(threads, h, TRUE, INFINITE);
    for (int t = 0; t < threads; t++) CloseHandle(h[t]);
    CloseHandle(g_go);
    fclose(g_log);
    g_log = nullptr;
    static bool seen[threads][500];
    const char* log = ReadAll(L"lines.log");
    int lines = 0, bad = 0;
    for (const char* s = log; *s;) {
        const char* e = strchr(s, '\n');
        size_t n = e ? (size_t)(e - s) : strlen(s), length = n;
        if (length && s[length - 1] == '\r') length--;
        unsigned hh = 0, mm = 0, ss = 0, t = 0, i = 0;
        char payload[128] = "";
        char line[192] = "";
        if (length < sizeof line) memcpy(line, s, length);
        bool ok = length < sizeof line && sscanf_s(line, "%2u:%2u:%2u thread %2u line %3u %127s", &hh, &mm, &ss, &t, &i, payload,
                                              (unsigned)sizeof payload) == 6 &&
                  t < threads && i < 500 && !seen[t][i] &&
                  strcmp(payload, "abcdefghijklmnopqrstuvwxyz0123456789abcdefghijklmnopqrstuvwxyz0123456789") == 0 &&
                  length == 9 + 19 + 72;
        if (ok) seen[t][i] = true;
        bad += !ok;
        lines++;
        s = e ? e + 1 : s + n;
    }
    CHECK_EQ(lines, threads * 500);
    CHECK_EQ(bad, 0);
}

static bool CopyTo(const wchar_t* from, const wchar_t* folder, const wchar_t* name) {
    wchar_t path[MAX_PATH];
    _snwprintf_s(path, _countof(path), _TRUNCATE, L"%s%s", folder, name);
    return CopyFileW(from, path, FALSE) != 0;
}

static void TestLoadMods() {
    wchar_t mods[MAX_PATH];
    _snwprintf_s(mods, _countof(mods), _TRUNCATE, L"%smods\\", g_tmp);
    CreateDirectoryW(mods, nullptr);
    CHECK(CopyTo(L"TestModOk.dll", mods, L"a_ok.asi"));
    CHECK(CopyTo(L"TestModCrash.dll", mods, L"b_crash.asi"));
    CHECK(CopyTo(L"TestModNoInit.dll", mods, L"c_noinit.asi"));
    CHECK(CopyTo(L"TestModOk.dll", mods, L"d_off.asi_off"));
    FILE* f = nullptr;
    wchar_t text[MAX_PATH];
    _snwprintf_s(text, _countof(text), _TRUNCATE, L"%se_text.asi", mods);
    if (!_wfopen_s(&f, text, L"w") && f) { fputs("not a DLL", f); fclose(f); }
    wcscpy_s(g_dir, g_tmp);
    g_api.gameBase = (uint8_t*)GetModuleHandleW(nullptr);
    LoadMods(nullptr);
    fclose(g_log);
    g_log = nullptr;
    const char* log = ReadAll(L"mods\\ModLoader.log");
    CHECK(strstr(log, "ModLoader test - loading mods\\*.asi") != nullptr);
    CHECK(strstr(log, "a_ok.asi: AvengersModInit returned 7") != nullptr);
    CHECK(strstr(log, "b_crash.asi: AvengersModInit crashed (0xC0000005)") != nullptr);
    CHECK(strstr(log, "c_noinit.asi: loaded (no AvengersModInit)") != nullptr);
    CHECK(strstr(log, "e_text.asi: could not load (error ") != nullptr);
    CHECK(strstr(log, "d_off") == nullptr);
    CHECK(strstr(log, "done: 3 mod(s) loaded") != nullptr);
    HMODULE ok = GetModuleHandleW(L"a_ok.asi");
    typedef const void* (*ApiFn)();
    ApiFn api = ok ? (ApiFn)GetProcAddress(ok, "TestModApi") : nullptr;
    CHECK(api && api() == &g_api);
}

int main() {
    wchar_t base[MAX_PATH];
    GetTempPathW(MAX_PATH, base);
    _snwprintf_s(g_tmp, _countof(g_tmp), _TRUNCATE, L"%sModLoaderTests-%lu\\", base, GetCurrentProcessId());
    CreateDirectoryW(g_tmp, nullptr);
    TestApi();
    TestPatchRace();
    TestLogLines();
    TestLoadMods();
    return TestSummary("ModLoaderTests");
}
