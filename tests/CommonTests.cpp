#define MOD_NAME L"CommonTests"
#include "../src/Common/CharacterTiles.h"
#include "Check.h"
#include "FakeImage.h"

using namespace tiles;

static void ModMain() {}

static wchar_t g_tmp[MAX_PATH];

static void MakeTempFolder() {
    wchar_t base[MAX_PATH];
    GetTempPathW(MAX_PATH, base);
    _snwprintf_s(g_tmp, _countof(g_tmp), _TRUNCATE, L"%sCommonTests-%lu\\", base, GetCurrentProcessId());
    CreateDirectoryW(g_tmp, nullptr);
}

static void ReadText(const wchar_t* path, char* out, size_t n) {
    out[0] = 0;
    FILE* f = nullptr;
    if (_wfopen_s(&f, path, L"rb") || !f) return;
    size_t got = fread(out, 1, n - 1, f);
    out[got] = 0;
    fclose(f);
}

static const char* LogText() {
    static char text[1 << 16];
    wchar_t path[MAX_PATH];
    _snwprintf_s(path, _countof(path), _TRUNCATE, L"%sCommonTests.log", g_tmp);
    ReadText(path, text, sizeof text);
    return text;
}

static DWORD Rights(const void* p) {
    MEMORY_BASIC_INFORMATION m = {};
    VirtualQuery(p, &m, sizeof m);
    return m.Protect;
}

static void TestLog() {
    wcscpy_s(g_dir, g_tmp);
    Log("line %d", 7);
    CHECK(strstr(LogText(), "line 7") != nullptr);

    for (int i = 0; i < 250; i++) g_dir[i] = L'x';
    g_dir[250] = L'\\'; g_dir[251] = 0;
    Log("never written (long path)");
    g_dir[0] = 0;
    Log("never written (no folder)");
    wcscpy_s(g_dir, g_tmp);
    CHECK(strstr(LogText(), "never written") == nullptr);
}

static void TestInvalidParameter() {
    wcscpy_s(g_dir, g_tmp);
    _set_invalid_parameter_handler(OnInvalidParameter);
    wchar_t tooSmall[4];
    const wchar_t* text = L"longer than the buffer";
    int r = swprintf_s(tooSmall, L"%s", text);
    CHECK_EQ(r, -1);
    CHECK(strstr(LogText(), "CRT: invalid parameter") != nullptr);
}

static HANDLE g_go;

struct PatchJob { int thread, threads, blocks; int ok; };

static DWORD WINAPI PatchThread(LPVOID p) {
    PatchJob* job = (PatchJob*)p;
    static const uint8_t nop2[2] = {0x90, 0x90}, int2[2] = {0xCC, 0xCC};
    WaitForSingleObject(g_go, INFINITE);
    for (int i = 0; i < job->blocks; i++)
        job->ok += PatchCode(0x1000 + (uintptr_t)(i * job->threads + job->thread) * 8, nop2, int2, 2);
    return 0;
}

static void TestPatchCode() {
    uint8_t* code = (uint8_t*)VirtualAlloc(nullptr, 0x4000, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    memset(code, 0x90, 0x4000);
    DWORD old;
    VirtualProtect(code, 0x4000, PAGE_EXECUTE_READ, &old);
    g_base = (uintptr_t)code;
    static const uint8_t nop2[2] = {0x90, 0x90}, int2[2] = {0xCC, 0xCC}, other[2] = {0x12, 0x34};
    CHECK(PatchCode(0x10, nop2, int2, 2));
    CHECK(code[0x10] == 0xCC && code[0x11] == 0xCC && code[0x12] == 0x90 && code[0x0F] == 0x90);
    CHECK_EQ(Rights(code), PAGE_EXECUTE_READ);
    CHECK(PatchCode(0x10, nop2, int2, 2));
    CHECK(!PatchCode(0x20, other, int2, 2));
    CHECK(code[0x20] == 0x90 && code[0x21] == 0x90);
    CHECK(!PatchCode(0x17, nop2, int2, 2));
    CHECK(!PatchCode(0x18, nop2, int2, 0));

    const int threads = 8, blocks = 128;
    PatchJob jobs[threads];
    HANDLE h[threads];
    g_go = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    for (int t = 0; t < threads; t++) {
        jobs[t] = { t, threads, blocks, 0 };
        h[t] = CreateThread(nullptr, 0, PatchThread, &jobs[t], 0, nullptr);
    }
    SetEvent(g_go);
    WaitForMultipleObjects(threads, h, TRUE, INFINITE);
    int ok = 0;
    for (int t = 0; t < threads; t++) { ok += jobs[t].ok; CloseHandle(h[t]); }
    CloseHandle(g_go);
    CHECK_EQ(ok, threads * blocks);
    int patched = 0;
    for (int i = 0; i < threads * blocks; i++) patched += code[0x1000 + i * 8] == 0xCC && code[0x1001 + i * 8] == 0xCC;
    CHECK_EQ(patched, threads * blocks);
    CHECK_EQ(Rights(code + 0x1000), PAGE_EXECUTE_READ);
    CHECK_EQ(Rights(code + 0x2000), PAGE_EXECUTE_READ);
    VirtualFree(code, 0, MEM_RELEASE);
}

struct SwapJob { uintptr_t* slot; int flips; int failed; };

static DWORD WINAPI SwapThread(LPVOID p) {
    SwapJob* job = (SwapJob*)p;
    WaitForSingleObject(g_go, INFINITE);
    for (int i = 0; i < job->flips; i++) {
        job->failed += !SwapPointer(job->slot, 0xA, 0xB);
        job->failed += !SwapPointer(job->slot, 0xB, 0xA);
    }
    return 0;
}

static void TestSwapPointer() {
    uintptr_t* table = (uintptr_t*)VirtualAlloc(nullptr, 0x1000, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    for (int i = 0; i < 64; i++) table[i] = 0x1000 + (uintptr_t)i;
    for (int i = 8; i < 16; i++) table[i] = 0xA;
    DWORD old;
    VirtualProtect(table, 0x1000, PAGE_READONLY, &old);
    CHECK(SwapPointer(table + 1, 0x1001, 0xBEEF));
    CHECK_EQ(table[1], 0xBEEF);
    CHECK(!SwapPointer(table + 2, 0x9999, 0xBEEF));
    CHECK_EQ(table[2], 0x1002);
    CHECK(!SwapPointer((uintptr_t*)((uint8_t*)table + 4), 0, 1));
    CHECK(SetPointer(table + 3, (void*)0xCAFE));
    CHECK_EQ(table[3], 0xCAFE);
    CHECK_EQ(Rights(table), PAGE_READONLY);

    const int threads = 8;
    SwapJob jobs[threads];
    HANDLE h[threads];
    g_go = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    for (int t = 0; t < threads; t++) {
        jobs[t] = { table + 8 + t, 2000, 0 };
        h[t] = CreateThread(nullptr, 0, SwapThread, &jobs[t], 0, nullptr);
    }
    SetEvent(g_go);
    WaitForMultipleObjects(threads, h, TRUE, INFINITE);
    int failed = 0;
    for (int t = 0; t < threads; t++) { failed += jobs[t].failed; CloseHandle(h[t]); }
    CloseHandle(g_go);
    CHECK_EQ(failed, 0);
    for (int t = 0; t < threads; t++) CHECK_EQ(table[8 + t], 0xA);
    CHECK_EQ(Rights(table), PAGE_READONLY);
    VirtualFree(table, 0, MEM_RELEASE);
}

struct FakeColumn {
    uint8_t w[0x300];
    int ids[8];
    uint8_t nav[0x100];
    void* vtbl[80];
};

static int g_refreshes, g_runs;
static bool g_done, g_finallyRan;

static void __fastcall FakeRefresh(uint8_t*) { g_refreshes++; }
static bool StubDone() { return g_done; }
static void StubRun() { g_runs++; }

static void FaultingRun() {
    __try {
        volatile int* nowhere = nullptr;
        *nowhere = 1;
    } __finally {
        g_finallyRan = true;
    }
}

static void Lay(FakeColumn& c, int count, int selected) {
    memset(&c, 0, sizeof c);
    c.vtbl[SLOT_REFRESH] = (void*)FakeRefresh;
    *(void**)c.w = c.vtbl;
    *(uint32_t*)(c.w + 0x238) = (uint32_t)count;
    *(int**)(c.w + 0x240) = c.ids;
    *(uint8_t**)(c.w + 0x188) = c.nav;
    *(int*)(c.nav + 0x84) = selected;
    c.ids[0] = 0xC7; c.ids[1] = SKILL_TILE_ID; c.ids[2] = 0xCE;
}

static void TestTileAction() {
    wcscpy_s(g_dir, g_tmp);
    FakeColumn c;
    Lay(c, 3, 1);
    TileButton b = { SKILL_TILE_ID, "IDLE", "DONE", "test tile", StubDone, StubRun };
    g_runs = g_refreshes = 0; g_done = false;
    CHECK(!TileAction(b, c.w, 0x1A2));
    *(int*)(c.nav + 0x84) = 0;
    CHECK(!TileAction(b, c.w, ACCEPT));
    *(int*)(c.nav + 0x84) = 1;
    CHECK(TileAction(b, c.w, ACCEPT));
    CHECK_EQ(g_runs, 0);
    CHECK(b.armedAt != 0);
    CHECK_EQ(g_refreshes, 1);
    CHECK(strcmp(TileTitle(b, false), "CONFIRM?") == 0);
    CHECK(TileAction(b, c.w, ACCEPT));
    CHECK_EQ(g_runs, 0);
    Sleep(DOUBLE_CLICK_MS + 50);
    CHECK(TileAction(b, c.w, ACCEPT));
    CHECK_EQ(g_runs, 1);
    CHECK(b.armedAt == 0);
    Sleep(DOUBLE_CLICK_MS + 50);
    g_done = true;
    CHECK(TileAction(b, c.w, ACCEPT));
    CHECK_EQ(g_runs, 1);
    CHECK(strcmp(TileTitle(b, true), "DONE") == 0);
    b.doneLabel = [] { return "OTHER"; };
    CHECK(strcmp(TileTitle(b, true), "OTHER") == 0);
    b.doneLabel = nullptr;

    Sleep(DOUBLE_CLICK_MS + 50);
    g_done = false;
    b.faulted = true;
    CHECK(TileAction(b, c.w, ACCEPT));
    CHECK_EQ(g_runs, 1);
    b.faulted = false;

    FakeColumn bad;
    Lay(bad, 3, 1);
    *(int**)(bad.w + 0x240) = (int*)0x10;
    CHECK(!TileAction(b, bad.w, ACCEPT));

    TileButton f = { SKILL_TILE_ID, "IDLE", "DONE", "faulting tile", StubDone, FaultingRun };
    g_finallyRan = false; g_done = false;
    CHECK(TileAction(f, c.w, ACCEPT));
    Sleep(DOUBLE_CLICK_MS + 50);
    CHECK(TileAction(f, c.w, ACCEPT));
    CHECK(f.faulted);
    CHECK(g_finallyRan);
    CHECK(strstr(LogText(), "fault during faulting tile - it stopped part-way") != nullptr);
}

static int g_setStates, g_drawnIndex, g_drawnAction, g_drawnSelected;
static const char* g_drawnTitle;

static void __fastcall FakeSetState(uint8_t*, uint32_t idx, uint8_t, uint8_t selected, uint8_t, const char* title, int action,
                                    const char*) {
    g_setStates++;
    g_drawnIndex = (int)idx;
    g_drawnSelected = selected;
    g_drawnTitle = title;
    g_drawnAction = action;
}
static const char* __fastcall FakeLocalize(void*, const char*) { return "Select"; }

static void TestTileRefresh() {
    FakeImage image(0x4E10000);
    static const uint8_t table[8] = {};
    CHECK(image.Jump(RVA_SETSTATE, (const void*)FakeSetState));
    CHECK(image.Jump(RVA_LOCALIZE, (const void*)FakeLocalize));
    CHECK(image.Put(RVA_LOCTABLE, table, sizeof table, PAGE_READONLY));
    g_base = (uintptr_t)image.base;
    FakeColumn c;
    Lay(c, 3, 1);
    *(void**)(c.w + 0xB8) = c.nav;
    TileButton b = { SKILL_TILE_ID, "IDLE", "DONE", "drawn tile", StubDone, StubRun };
    g_done = false; g_setStates = 0;
    TileRefresh(b, c.w);
    CHECK_EQ(g_setStates, 1);
    CHECK_EQ(g_drawnIndex, 1);
    CHECK_EQ(g_drawnSelected, 1);
    CHECK(g_drawnTitle && strcmp(g_drawnTitle, "IDLE") == 0);
    CHECK_EQ(g_drawnAction, ACCEPT);
    g_done = true; b.doneAt = 0;
    b.doneLabel = [] { return "ALL GIVEN"; };
    TileRefresh(b, c.w);
    CHECK(g_drawnTitle && strcmp(g_drawnTitle, "ALL GIVEN") == 0);
    CHECK_EQ(g_drawnAction, 0);
    *(void**)(c.w + 0xB8) = nullptr;
    TileRefresh(b, c.w);
    CHECK_EQ(g_setStates, 2);
    *(void**)(c.w + 0xB8) = c.nav;
    b.faulted = true;
    TileRefresh(b, c.w);
    CHECK_EQ(g_setStates, 2);
}

static void TestHelpers() {
    FakeColumn c;
    Lay(c, 3, 2);
    CHECK_EQ(IndexOf(c.w, 0xCE), 2);
    CHECK_EQ(IndexOf(c.w, 0x1234), -1);
    CHECK(IsOwnAccept(c.w, 0xCE, ACCEPT));
    CHECK(!IsOwnAccept(c.w, SKILL_TILE_ID, ACCEPT));
    CHECK(!IsOwnAccept(c.w, 0xCE, 0x1A2));
    ULONGLONG last = 0;
    CHECK(!SamePress(last));
    CHECK(SamePress(last));
    Sleep(DOUBLE_CLICK_MS + 50);
    CHECK(!SamePress(last));
    CHECK_EQ(InventoryPart(0), 0);
    static uint8_t entity[ENTITY_INVENTORY_PART + 8];
    *(uintptr_t*)(entity + ENTITY_INVENTORY_PART) = 0x1234;
    CHECK_EQ(InventoryPart((uintptr_t)entity), 0x1234);
    TileLock a;
    CHECK(a.ok());
    TileLock again;
    CHECK(again.ok());
}

static void __fastcall OurLayout(uint8_t*) {}
static void __fastcall OurRefresh(uint8_t*) { g_refreshes += 2; }
static void __fastcall OurAction(uint8_t*, int, uint8_t) { g_runs += 2; }

struct ChainDll { HMODULE m; uintptr_t layout, refresh, action; };

static ChainDll LoadChain(const wchar_t* file) {
    ChainDll d = {};
    d.m = LoadLibraryW(file);
    if (d.m) {
        d.layout = (uintptr_t)GetProcAddress(d.m, "ChainLayout");
        d.refresh = (uintptr_t)GetProcAddress(d.m, "ChainRefresh");
        d.action = (uintptr_t)GetProcAddress(d.m, "ChainAction");
    }
    return d;
}

static uint8_t* g_vtPage;
static uintptr_t* g_vt;

static void SetSlots(uintptr_t layout, uintptr_t refresh, uintptr_t action) {
    DWORD old;
    VirtualProtect(g_vtPage, 0x1000, PAGE_READWRITE, &old);
    g_vt[SLOT_LAYOUT] = layout; g_vt[SLOT_REFRESH] = refresh; g_vt[SLOT_ACTION] = action;
    VirtualProtect(g_vtPage, 0x1000, PAGE_READONLY, &old);
}

static void TestInstallTile() {
    wcscpy_s(g_dir, g_tmp);
    ChainDll a = LoadChain(L"ChainTileA.dll"), b = LoadChain(L"ChainTileB.dll"), oldVersion = LoadChain(L"ChainTileOld.dll");
    CHECK(a.m && b.m && oldVersion.m && a.layout && b.refresh);
    uint8_t* page = (uint8_t*)VirtualAlloc(nullptr, 0x1000, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    g_vtPage = page;
    g_vt = (uintptr_t*)(page + 0x100);
    g_base = (uintptr_t)g_vt - RVA_VTBL;
    uintptr_t gl = g_base + RVA_LAYOUT, gr = g_base + RVA_REFRESH, ga = g_base + RVA_ACTION;
    TileHooks next = {};
    SlotOwnerKind under = OWNER_UNKNOWN;

    SetSlots(gl, gr, ga);
    CHECK(InstallTile(OWNER_SKILL, OurLayout, OurRefresh, OurAction, next, under) == nullptr);
    CHECK_EQ(under, OWNER_GAME);
    CHECK((uintptr_t)next.layout == gl && (uintptr_t)next.refresh == gr && (uintptr_t)next.action == ga);
    CHECK(g_vt[SLOT_LAYOUT] == (uintptr_t)OurLayout && g_vt[SLOT_ACTION] == (uintptr_t)OurAction);

    CHECK(InstallTile(OWNER_SKILL, OurLayout, OurRefresh, OurAction, next, under) != nullptr);

    CHECK(InstallTile(OWNER_COLLECTION, OurLayout, OurRefresh, OurAction, next, under) == nullptr);
    CHECK_EQ(under, OWNER_SKILL);

    SetSlots(a.layout, a.refresh, a.action);
    CHECK(InstallTile(OWNER_COLLECTION, OurLayout, OurRefresh, OurAction, next, under) == nullptr);
    CHECK_EQ(under, OWNER_CHAIN);
    CHECK((uintptr_t)next.action == a.action);

    uintptr_t k = (uintptr_t)GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "GetTickCount");
    SetSlots(k, k, k);
    CHECK(InstallTile(OWNER_SKILL, OurLayout, OurRefresh, OurAction, next, under) != nullptr);
    CHECK(g_vt[SLOT_LAYOUT] == k);
    SetSlots(oldVersion.layout, oldVersion.refresh, oldVersion.action);
    CHECK(InstallTile(OWNER_SKILL, OurLayout, OurRefresh, OurAction, next, under) != nullptr);

    SetSlots(a.layout, gr, ga);
    CHECK(InstallTile(OWNER_SKILL, OurLayout, OurRefresh, OurAction, next, under) != nullptr);
    SetSlots(a.layout, b.refresh, a.action);
    CHECK(InstallTile(OWNER_SKILL, OurLayout, OurRefresh, OurAction, next, under) != nullptr);
    CHECK(g_vt[SLOT_REFRESH] == b.refresh);
    VirtualFree(page, 0, MEM_RELEASE);
}

static void TestInstallRollback() {
    wchar_t file[MAX_PATH];
    _snwprintf_s(file, _countof(file), _TRUNCATE, L"%sview.bin", g_tmp);
    const size_t VIEW = 0x10000;
    uint8_t* x = (uint8_t*)VirtualAlloc(nullptr, 2 * VIEW, MEM_RESERVE, PAGE_NOACCESS);
    VirtualFree(x, 0, MEM_RELEASE);
    uintptr_t* vt = (uintptr_t*)(x + VIEW - 0x100);
    uintptr_t base = (uintptr_t)vt - RVA_VTBL;
    static uint8_t content[0x10000];
    *(uintptr_t*)(content + VIEW - 0x100 + SLOT_ACTION * 8) = base + RVA_ACTION;
    FILE* f = nullptr;
    _wfopen_s(&f, file, L"wb");
    if (f) { fwrite(content, 1, sizeof content, f); fclose(f); }
    HANDLE fh = CreateFileW(file, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
    HANDLE map = CreateFileMappingW(fh, nullptr, PAGE_READONLY, 0, 0, nullptr);
    void* view = MapViewOfFileEx(map, FILE_MAP_READ, 0, 0, VIEW, x);
    uint8_t* next = (uint8_t*)VirtualAlloc(x + VIEW, 0x1000, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    CHECK(view == x && next == x + VIEW);
    if (view != x || next != x + VIEW) return;
    vt[SLOT_LAYOUT] = base + RVA_LAYOUT;
    vt[SLOT_REFRESH] = base + RVA_REFRESH;
    DWORD old;
    VirtualProtect(next, 0x1000, PAGE_READONLY, &old);
    g_base = base;
    TileHooks hooks = {};
    SlotOwnerKind under = OWNER_UNKNOWN;
    const char* err = InstallTile(OWNER_SKILL, OurLayout, OurRefresh, OurAction, hooks, under);
    CHECK(err != nullptr && strstr(err, "safely") != nullptr);
    CHECK(vt[SLOT_LAYOUT] == base + RVA_LAYOUT);
    CHECK(vt[SLOT_REFRESH] == base + RVA_REFRESH);
    CHECK(vt[SLOT_ACTION] == base + RVA_ACTION);
    UnmapViewOfFile(view);
    CloseHandle(map);
    CloseHandle(fh);
    VirtualFree(next, 0, MEM_RELEASE);
    DeleteFileW(file);
}

int main() {
    MakeTempFolder();
    TestLog();
    TestInvalidParameter();
    TestPatchCode();
    TestSwapPointer();
    TestTileAction();
    TestTileRefresh();
    TestHelpers();
    TestInstallTile();
    TestInstallRollback();
    wchar_t log[MAX_PATH];
    _snwprintf_s(log, _countof(log), _TRUNCATE, L"%sCommonTests.log", g_tmp);
    DeleteFileW(log);
    RemoveDirectoryW(g_tmp);
    return TestSummary("CommonTests");
}
