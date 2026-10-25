#define MOD_VERSION "test"
#include "../src/QuickStart/QuickStart.cpp"
#include "Check.h"
#include "FakeImage.h"
#include <initializer_list>

static const size_t IMAGE_SIZE = 0x4E10000;

static const uintptr_t RVA_FOCUS_AREA = 0x12D6960;
static const uint8_t REAL_FOCUS[24] = { 0x68, 0x48, 0x83, 0xC4, 0x50, 0x41, 0x5E, 0xC3,
                                        0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC,
                                        0x40, 0x53, 0x55, 0x57, 0x41, 0x56, 0x41, 0x57 };

struct RealBlock { uintptr_t rva; uint8_t bytes[8]; };
static const RealBlock REAL_EPI1 = { 0x21E1790, { 0x44, 0x24, 0x48, 0xC4, 0x00, 0x00, 0x00, 0xE8 } };
static const RealBlock REAL_EPI2 = { 0x2228BC0, { 0xC4, 0x00, 0x00, 0x00, 0xE8, 0xD7, 0x75, 0xF3 } };
static const RealBlock REAL_EPI3 = { 0x21F8B08, { 0x30, 0xC4, 0x00, 0x00, 0x00, 0xE8, 0x8E, 0x76 } };
static const RealBlock REAL_LEGAL = { 0x21DF9F8, { 0x00, 0x40, 0x40, 0x45, 0x33, 0xC0, 0x41, 0x8D } };
static const RealBlock REAL_INTRO = { 0x2227ED8, { 0x48, 0x28, 0x00, 0x00, 0x00, 0xE8, 0xBE, 0x82 } };

static wchar_t g_tmp[MAX_PATH];

static const char* LogText() {
    static char text[1 << 16];
    text[0] = 0;
    wchar_t path[MAX_PATH];
    _snwprintf_s(path, _countof(path), _TRUNCATE, L"%sQuickStart.log", g_tmp);
    FILE* f = nullptr;
    if (_wfopen_s(&f, path, L"rb") || !f) return text;
    size_t got = fread(text, 1, sizeof text - 1, f);
    text[got] = 0;
    fclose(f);
    return text;
}
static void ClearLog() {
    wchar_t path[MAX_PATH];
    _snwprintf_s(path, _countof(path), _TRUNCATE, L"%sQuickStart.log", g_tmp);
    DeleteFileW(path);
}
static bool Logged(const char* text) { return strstr(LogText(), text) != nullptr; }

static bool Holds(const FakeImage& im, uintptr_t rva, const void* bytes, size_t n) { return memcmp(im.At(rva), bytes, n) == 0; }

static int g_updates, g_presses;
static uint8_t* g_pressedState;
static uint8_t g_menuManager[0x900];
static uint8_t* g_menuManagerPtr = g_menuManager;

static void __fastcall FakeUpdate(uint8_t*, float) { g_updates++; }
static void __fastcall FakePress(uint8_t* state, uint8_t) { g_presses++; g_pressedState = state; }

static void LayReal(FakeImage& im) {
    CHECK(im.Put(RVA_FOCUS_AREA, REAL_FOCUS, sizeof REAL_FOCUS, PAGE_EXECUTE_READ));
    for (const RealBlock* b : { &REAL_EPI1, &REAL_EPI2, &REAL_EPI3, &REAL_LEGAL, &REAL_INTRO })
        CHECK(im.Put(b->rva, b->bytes, 8, PAGE_EXECUTE_READ));
    CHECK(im.Jump(RVA_PRESSSTART_UPDATE, (const void*)FakeUpdate));
    CHECK(im.Jump(RVA_PRESSSTART_GO, (const void*)FakePress));
    uintptr_t game = (uintptr_t)im.base + RVA_PRESSSTART_UPDATE;
    CHECK(im.Put(RVA_PRESSSTART_SLOT, &game, 8, PAGE_READONLY));
    CHECK(im.Put(RVA_MENUMGR, &g_menuManagerPtr, 8, PAGE_READWRITE));
    g_base = (uintptr_t)im.base;
}

static void TestFocusOnRealBytes() {
    static const uint8_t patched[24] = { 0x68, 0x48, 0x83, 0xC4, 0x50, 0x41, 0x5E, 0xC3,
                                         0x31, 0xC0, 0xC3, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC,
                                         0xEB, 0xF6, 0x55, 0x57, 0x41, 0x56, 0x41, 0x57 };
    FakeImage im(IMAGE_SIZE);
    LayReal(im);
    ClearLog();
    NoFocusPause();
    CHECK(Holds(im, RVA_FOCUS_AREA, patched, sizeof patched));
    CHECK_EQ(im.Rights(RVA_CANPAUSE), PAGE_EXECUTE_READ);
    CHECK(Logged("focus: the game keeps running in the background"));

    CHECK_EQ(RVA_CANPAUSE + 2 - 10, RVA_CANPAUSE_PAD);
    NoFocusPause();
    CHECK(Holds(im, RVA_FOCUS_AREA, patched, sizeof patched));
}

static void TestFocusAfterVersion10() {
    uint8_t v10[24];
    memcpy(v10, REAL_FOCUS, sizeof v10);
    static const uint8_t old[6] = { 0x31, 0xC0, 0xC3, 0xCC, 0xCC, 0xCC };
    memcpy(v10 + 16, old, sizeof old);
    FakeImage im(IMAGE_SIZE);
    LayReal(im);
    CHECK(im.Put(RVA_FOCUS_AREA, v10, sizeof v10, PAGE_EXECUTE_READ));
    ClearLog();
    NoFocusPause();
    CHECK(Holds(im, RVA_FOCUS_AREA, v10, sizeof v10));
    CHECK(Logged("focus: already patched by Quick Start 1.0"));
}

static void TestFocusOtherCode() {
    uint8_t other[24];
    memcpy(other, REAL_FOCUS, sizeof other);
    static const uint8_t start[8] = { 0x48, 0x89, 0x5C, 0x24, 0x08, 0x57, 0x48, 0x83 };
    memcpy(other + 16, start, sizeof start);
    {
        FakeImage im(IMAGE_SIZE);
        LayReal(im);
        CHECK(im.Put(RVA_FOCUS_AREA, other, sizeof other, PAGE_EXECUTE_READ));
        ClearLog();
        NoFocusPause();
        CHECK(Holds(im, RVA_FOCUS_AREA, other, sizeof other));
        CHECK(Logged("focus: code differs - left as it is"));
    }

    uint8_t noPad[24];
    memcpy(noPad, REAL_FOCUS, sizeof noPad);
    memset(noPad + 8, 0x90, 8);
    {
        FakeImage im(IMAGE_SIZE);
        LayReal(im);
        CHECK(im.Put(RVA_FOCUS_AREA, noPad, sizeof noPad, PAGE_EXECUTE_READ));
        ClearLog();
        NoFocusPause();
        CHECK(Holds(im, RVA_FOCUS_AREA, noPad, sizeof noPad));
        CHECK(Logged("focus: code differs - left as it is"));
    }
}

typedef int (*IntFn)();
static volatile LONG g_stop;
static volatile LONG g_ones, g_zeros, g_others;

static DWORD WINAPI CallLoop(LPVOID p) {
    IntFn f = (IntFn)p;
    LONG ones = 0, zeros = 0, others = 0;
    while (!g_stop) {
        int r = f();
        if (r == 1) ones++; else if (r == 0) zeros++; else others++;
    }
    InterlockedAdd(&g_ones, ones);
    InterlockedAdd(&g_zeros, zeros);
    InterlockedAdd(&g_others, others);
    return 0;
}

static void TestFocusWhileRunning() {
    uint8_t code[24];
    memcpy(code, REAL_FOCUS, 16);
    static const uint8_t standIn[8] = { 0x40, 0x53, 0xB8, 0x01, 0x00, 0x00, 0x00, 0x5B };
    memcpy(code + 16, standIn, sizeof standIn);
    static const uint8_t ret[1] = { 0xC3 };
    FakeImage im(IMAGE_SIZE);
    LayReal(im);
    CHECK(im.Put(RVA_FOCUS_AREA, code, sizeof code, PAGE_EXECUTE_READ));
    CHECK(im.Put(RVA_CANPAUSE + 8, ret, 1, PAGE_EXECUTE_READ));
    IntFn f = (IntFn)im.At(RVA_CANPAUSE);
    CHECK_EQ(f(), 1);
    const int threads = 4;
    HANDLE h[threads];
    g_stop = 0; g_ones = g_zeros = g_others = 0;
    for (int t = 0; t < threads; t++) h[t] = CreateThread(nullptr, 0, CallLoop, (LPVOID)f, 0, nullptr);
    Sleep(100);
    NoFocusPause();
    Sleep(100);
    g_stop = 1;
    WaitForMultipleObjects(threads, h, TRUE, INFINITE);
    for (int t = 0; t < threads; t++) CloseHandle(h[t]);
    CHECK(g_ones > 0);
    CHECK(g_zeros > 0);
    CHECK_EQ(g_others, 0);
    CHECK_EQ(f(), 0);
}

static void CheckBlock(const FakeImage& im, const RealBlock& b, size_t at, const uint8_t* with, size_t n, bool patched) {
    uint8_t want[8];
    memcpy(want, b.bytes, 8);
    if (patched) memcpy(want + at, with, n);
    CHECK(Holds(im, b.rva, want, 8));
}

static const uint8_t NEW_02[4] = { 0x02, 0, 0, 0 }, NEW_04[4] = { 0x04, 0, 0, 0 }, NEW_F0[2] = { 0, 0 };

static void CheckEpilepsy(const FakeImage& im, bool patched) {
    CheckBlock(im, REAL_EPI1, 3, NEW_02, 4, patched);
    CheckBlock(im, REAL_EPI2, 0, NEW_02, 4, patched);
    CheckBlock(im, REAL_EPI3, 1, NEW_02, 4, patched);
}

static void TestScreensOnRealBytes() {
    FakeImage im(IMAGE_SIZE);
    LayReal(im);
    ClearLog();
    SkipScreens(true, true, true);
    CheckEpilepsy(im, true);
    CheckBlock(im, REAL_LEGAL, 1, NEW_F0, 2, true);
    CheckBlock(im, REAL_INTRO, 1, NEW_04, 4, true);
    CHECK(Logged("startup: epilepsy warning skipped"));
    CHECK(Logged("startup: legal screen wait removed"));
    CHECK(Logged("startup: intro movie skipped"));
    CHECK_EQ(im.Rights(REAL_EPI1.rva), PAGE_EXECUTE_READ);
    SkipScreens(true, true, true);
    CheckEpilepsy(im, true);
}

static void TestEpilepsyAllOrNone() {
    {
        FakeImage im(IMAGE_SIZE);
        LayReal(im);
        CHECK(im.Put(REAL_EPI2.rva, NEW_02, 4, PAGE_EXECUTE_READ));
        ClearLog();
        SkipScreens(true, false, false);
        CheckEpilepsy(im, true);
        CHECK(Logged("startup: epilepsy warning skipped"));
    }

    {
        FakeImage im(IMAGE_SIZE);
        LayReal(im);
        static const uint8_t c5[1] = { 0xC5 };
        CHECK(im.Put(REAL_EPI3.rva + 1, c5, 1, PAGE_EXECUTE_READ));
        ClearLog();
        SkipScreens(true, false, false);
        CheckBlock(im, REAL_EPI1, 3, NEW_02, 4, false);
        CheckBlock(im, REAL_EPI2, 0, NEW_02, 4, false);
        CHECK(Logged("startup: epilepsy warning code differs - left as it is"));
    }

    {
        FakeImage im(IMAGE_SIZE);
        CHECK(im.ReadOnlyView(REAL_EPI2.rva, REAL_EPI2.bytes, 8));
        CHECK(im.Put(REAL_EPI1.rva, REAL_EPI1.bytes, 8, PAGE_EXECUTE_READ));
        CHECK(im.Put(REAL_EPI3.rva, REAL_EPI3.bytes, 8, PAGE_EXECUTE_READ));
        g_base = (uintptr_t)im.base;
        DWORD old;
        CHECK(!VirtualProtect(im.At(REAL_EPI2.rva), 8, PAGE_EXECUTE_READWRITE, &old));
        ClearLog();
        SkipScreens(true, false, false);
        CheckEpilepsy(im, false);
        CHECK(Logged("startup: epilepsy warning code differs - left as it is"));
    }
}

static void TestPressStart() {
    FakeImage im(IMAGE_SIZE);
    LayReal(im);
    uintptr_t* slot = (uintptr_t*)im.At(RVA_PRESSSTART_SLOT);
    ClearLog();
    AutoPressStart();
    CHECK_EQ(*slot, (uintptr_t)PressStartUpdate);
    CHECK_EQ(im.Rights(RVA_PRESSSTART_SLOT), PAGE_READONLY);
    CHECK(Logged("startup: press start will be pressed for you"));
    UpdateFn update = (UpdateFn)*slot;
    uint8_t state[16];
    g_pressed = 0; g_frames = 0; g_updates = 0; g_presses = 0;
    *(int*)(g_menuManager + 0x888) = 2;
    update(state, 0.016f);
    CHECK_EQ(g_updates, 1);
    CHECK_EQ(g_presses, 0);
    *(int*)(g_menuManager + 0x888) = STATE_PRESS_START;
    update(state, 0.016f);
    update(state, 0.016f);
    CHECK_EQ(g_presses, 0);
    update(state, 0.016f);
    CHECK_EQ(g_presses, 1);
    CHECK(g_pressedState == state);
    for (int i = 0; i < 5; i++) update(state, 0.016f);
    CHECK_EQ(g_presses, 1);
    CHECK_EQ(g_updates, 9);

    g_pressed = 0; g_frames = 0;
    *(uint8_t**)im.At(RVA_MENUMGR) = (uint8_t*)0x10;
    ClearLog();
    update(state, 0.016f);
    CHECK_EQ(g_pressed, 1);
    CHECK(Logged("startup: fault pressing start - press it yourself"));
    CHECK_EQ(g_presses, 1);
}

static void TestPressStartOtherCode() {
    FakeImage other(IMAGE_SIZE);
    LayReal(other);
    uintptr_t elsewhere = 0x1234;
    CHECK(other.Put(RVA_PRESSSTART_SLOT, &elsewhere, 8, PAGE_READONLY));
    ClearLog();
    AutoPressStart();
    CHECK_EQ(*(uintptr_t*)other.At(RVA_PRESSSTART_SLOT), 0x1234);
    CHECK(Logged("startup: press start code differs - not automated"));
}

static void IniFile(wchar_t (&path)[MAX_PATH]) { _snwprintf_s(path, _countof(path), _TRUNCATE, L"%sQuickStart.ini", g_tmp); }

static void TestSettings() {
    wchar_t ini[MAX_PATH];
    IniFile(ini);
    DeleteFileW(ini);
    wcscpy_s(g_dir, g_tmp);
    ClearLog();
    WriteDefaultIni();
    char text[2048] = "";
    FILE* f = nullptr;
    if (!_wfopen_s(&f, ini, L"rb") && f) { text[fread(text, 1, sizeof text - 1, f)] = 0; fclose(f); }
    CHECK(strcmp(text, DEFAULT_INI) == 0);
    CHECK(Logged("settings: QuickStart.ini written with every part on"));
    for (const wchar_t* key : { L"NoPauseWhenUnfocused", L"SkipEpilepsyWarning", L"SkipLegalWait", L"SkipIntroMovie", L"AutoPressStart" })
        CHECK_EQ(Setting(key), 1);

    CHECK(WritePrivateProfileStringW(L"QuickStart", L"SkipIntroMovie", L"0", ini));
    CHECK_EQ(Setting(L"SkipIntroMovie"), 0);
    WriteDefaultIni();
    CHECK_EQ(Setting(L"SkipIntroMovie"), 0);

    DeleteFileW(ini);
    CHECK_EQ(Setting(L"SkipIntroMovie"), 1);
    g_dir[0] = 0;
    CHECK_EQ(Setting(L"SkipIntroMovie"), 1);
    WriteDefaultIni();
    CHECK(GetFileAttributesW(ini) == INVALID_FILE_ATTRIBUTES);
    wcscpy_s(g_dir, g_tmp);
}

static void TestModMain() {
    wchar_t ini[MAX_PATH];
    IniFile(ini);
    {
        DeleteFileW(ini);
        FakeImage im(IMAGE_SIZE);
        LayReal(im);
        ClearLog();
        ModMain();
        CHECK(GetFileAttributesW(ini) != INVALID_FILE_ATTRIBUTES);
        CHECK(Logged("Quick Start test"));
        CHECK(Logged("focus: the game keeps running in the background"));
        CheckEpilepsy(im, true);
        CheckBlock(im, REAL_LEGAL, 1, NEW_F0, 2, true);
        CheckBlock(im, REAL_INTRO, 1, NEW_04, 4, true);
        CHECK_EQ(*(uintptr_t*)im.At(RVA_PRESSSTART_SLOT), (uintptr_t)PressStartUpdate);
    }
    {
        for (const wchar_t* key : { L"NoPauseWhenUnfocused", L"SkipEpilepsyWarning", L"SkipLegalWait", L"SkipIntroMovie", L"AutoPressStart" })
            WritePrivateProfileStringW(L"QuickStart", key, L"0", ini);
        FakeImage im(IMAGE_SIZE);
        LayReal(im);
        ClearLog();
        ModMain();
        CHECK(Holds(im, RVA_FOCUS_AREA, REAL_FOCUS, sizeof REAL_FOCUS));
        CheckEpilepsy(im, false);
        CheckBlock(im, REAL_LEGAL, 1, NEW_F0, 2, false);
        CheckBlock(im, REAL_INTRO, 1, NEW_04, 4, false);
        CHECK_EQ(*(uintptr_t*)im.At(RVA_PRESSSTART_SLOT), (uintptr_t)im.base + RVA_PRESSSTART_UPDATE);
    }
    DeleteFileW(ini);
}

int main() {
    wchar_t base[MAX_PATH];
    GetTempPathW(MAX_PATH, base);
    _snwprintf_s(g_tmp, _countof(g_tmp), _TRUNCATE, L"%sQuickStartTests-%lu\\", base, GetCurrentProcessId());
    CreateDirectoryW(g_tmp, nullptr);
    wcscpy_s(g_dir, g_tmp);
    TestFocusOnRealBytes();
    TestFocusAfterVersion10();
    TestFocusOtherCode();
    TestFocusWhileRunning();
    TestScreensOnRealBytes();
    TestEpilepsyAllOrNone();
    TestPressStart();
    TestPressStartOtherCode();
    TestSettings();
    TestModMain();
    ClearLog();
    RemoveDirectoryW(g_tmp);
    return TestSummary("QuickStartTests");
}
