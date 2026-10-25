#define MOD_NAME L"QuickStart"
#include "..\Common\ModCommon.h"

static const uintptr_t RVA_CANPAUSE = 0x12D6970;
static const uintptr_t RVA_CANPAUSE_PAD = 0x12D6968;
static const uintptr_t RVA_EPI1 = 0x21E1793, RVA_EPI2 = 0x2228BC0, RVA_EPI3 = 0x21F8B09;
static const uintptr_t RVA_LEGALWAIT = 0x21DF9F9, RVA_INTRONEXT = 0x2227ED9;
static const uintptr_t RVA_PRESSSTART_SLOT = 0x2C1E490, RVA_PRESSSTART_UPDATE = 0x2228ED0, RVA_PRESSSTART_GO = 0x2213810;
static const uintptr_t RVA_MENUMGR = 0x4E04CE0;
static const int STATE_PRESS_START = 4;

typedef void (__fastcall *UpdateFn)(uint8_t*, float);
typedef void (__fastcall *PressFn)(uint8_t*, uint8_t);

static bool IniPath(wchar_t (&path)[MAX_PATH]) {
    return g_dir[0] && _snwprintf_s(path, _countof(path), _TRUNCATE, L"%sQuickStart.ini", g_dir) >= 0;
}

static const char DEFAULT_INI[] =
    "; Quick Start settings: 1 = on, 0 = off. Restart the game after a change.\r\n"
    "[QuickStart]\r\n"
    "; keep loading and running when the game window is not in front\r\n"
    "NoPauseWhenUnfocused=1\r\n"
    "; skip the photosensitivity (epilepsy) warning screen\r\n"
    "SkipEpilepsyWarning=1\r\n"
    "; don't wait 3 seconds on the legal screen\r\n"
    "SkipLegalWait=1\r\n"
    "; skip the intro movie\r\n"
    "SkipIntroMovie=1\r\n"
    "; press \"any key\" on the title screen for you, so the profile loads and the main menu opens\r\n"
    "AutoPressStart=1\r\n";

static void WriteDefaultIni() {
    wchar_t path[MAX_PATH];
    if (!IniPath(path)) return;
    HANDLE f = CreateFileW(path, GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f == INVALID_HANDLE_VALUE) return;
    DWORD wrote = 0;
    bool ok = WriteFile(f, DEFAULT_INI, (DWORD)(sizeof DEFAULT_INI - 1), &wrote, nullptr) && wrote == sizeof DEFAULT_INI - 1;
    CloseHandle(f);
    Log(ok ? "settings: QuickStart.ini written with every part on" : "settings: could not write QuickStart.ini");
}

static int Setting(const wchar_t* key) {
    wchar_t path[MAX_PATH];
    return IniPath(path) ? GetPrivateProfileIntW(L"QuickStart", key, 1, path) : 1;
}

static bool CodeIs(uintptr_t rva, const void* bytes, size_t n) { return memcmp((void*)(g_base + rva), bytes, n) == 0; }

static void NoFocusPause() {
    static const uint8_t pad[3] = {0xCC, 0xCC, 0xCC}, stub[3] = {0x31, 0xC0, 0xC3};
    static const uint8_t entry[2] = {0x40, 0x53}, jump[2] = {0xEB, 0xF6};
    static const uint8_t v10[6] = {0x31, 0xC0, 0xC3, 0xCC, 0xCC, 0xCC};
    if (CodeIs(RVA_CANPAUSE, v10, sizeof v10)) { Log("focus: already patched by Quick Start 1.0"); return; }
    if (!PatchCode(RVA_CANPAUSE_PAD, pad, stub, sizeof pad)) { Log("focus: code differs - left as it is"); return; }
    if (PatchCode(RVA_CANPAUSE, entry, jump, sizeof entry)) { Log("focus: the game keeps running in the background"); return; }
    PatchCode(RVA_CANPAUSE_PAD, stub, pad, sizeof pad);
    Log("focus: code differs - left as it is");
}

struct CodePatch { uintptr_t rva; const uint8_t* expect; const uint8_t* with; size_t n; };

static bool PatchAll(const CodePatch* p, int n) {
    for (int i = 0; i < n; i++)
        if (!CodeIs(p[i].rva, p[i].expect, p[i].n) && !CodeIs(p[i].rva, p[i].with, p[i].n)) return false;
    for (int i = 0; i < n; i++) {
        if (PatchCode(p[i].rva, p[i].expect, p[i].with, p[i].n)) continue;
        while (i--) PatchCode(p[i].rva, p[i].with, p[i].expect, p[i].n);
        return false;
    }
    return true;
}

static void SkipScreens(bool epilepsy, bool legal, bool intro) {
    static const uint8_t st_c4[4] = {0xC4, 0, 0, 0}, st_02[4] = {0x02, 0, 0, 0};
    static const uint8_t st_28[4] = {0x28, 0, 0, 0}, st_04[4] = {0x04, 0, 0, 0};
    static const uint8_t f3[2] = {0x40, 0x40}, f0[2] = {0, 0};
    if (epilepsy) {
        static const CodePatch epi[3] = { { RVA_EPI1, st_c4, st_02, 4 }, { RVA_EPI2, st_c4, st_02, 4 }, { RVA_EPI3, st_c4, st_02, 4 } };
        Log(PatchAll(epi, 3) ? "startup: epilepsy warning skipped" : "startup: epilepsy warning code differs - left as it is");
    }
    if (legal) Log(PatchCode(RVA_LEGALWAIT, f3, f0, 2) ? "startup: legal screen wait removed" : "startup: legal screen code differs");
    if (intro) Log(PatchCode(RVA_INTRONEXT, st_28, st_04, 4) ? "startup: intro movie skipped" : "startup: intro movie code differs");
}

static UpdateFn g_pressUpdate;
static volatile LONG g_pressed;
static int g_frames;

static void __fastcall PressStartUpdate(uint8_t* self, float dt) {
    g_pressUpdate(self, dt);
    if (g_pressed) return;
    __try {
        uint8_t* mgr = *(uint8_t**)(g_base + RVA_MENUMGR);
        if (!mgr || *(int*)(mgr + 0x888) != STATE_PRESS_START) return;
        if (++g_frames < 3) return;
        if (InterlockedExchange(&g_pressed, 1)) return;
        Log("startup: press start");
        ((PressFn)(g_base + RVA_PRESSSTART_GO))(self, 0);
    } FAULT { g_pressed = 1; Log("startup: fault pressing start - press it yourself"); }
}

static void AutoPressStart() {
    uintptr_t* slot = (uintptr_t*)(g_base + RVA_PRESSSTART_SLOT);
    uintptr_t game = g_base + RVA_PRESSSTART_UPDATE;
    if (*slot != game) { Log("startup: press start code differs - not automated"); return; }
    g_pressUpdate = (UpdateFn)game;
    if (!SwapPointer(slot, game, (uintptr_t)PressStartUpdate)) { Log("startup: could not change press start"); return; }
    Log("startup: press start will be pressed for you");
}

static void ModMain() {
    Log("Quick Start %s", MOD_VERSION);
    WriteDefaultIni();
    if (Setting(L"NoPauseWhenUnfocused")) NoFocusPause();
    SkipScreens(Setting(L"SkipEpilepsyWarning") != 0, Setting(L"SkipLegalWait") != 0, Setting(L"SkipIntroMovie") != 0);
    if (Setting(L"AutoPressStart")) AutoPressStart();
}
