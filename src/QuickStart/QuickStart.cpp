// Quick Start 1.0 - for Marvel's Avengers PC (Steam 2.8.2), by Space Trucker.
//  - Keeps the game running (loading, streaming) when its window is not in front.
//  - Skips the startup screens: epilepsy warning, the legal screen's wait, the intro movie and
//    "press any key". The profile and save still load the normal way, then the main menu opens.
// Each part can be switched off in QuickStart.ini next to the .asi. Log: QuickStart.log.
//
// Game code (static analysis; STARTUP-FOCUS-FINDINGS.md):
//   PCGameShell::Update FUN_1412d9520 pushes pause group 0x11 "SystemPause" and then loops
//   while (GetForegroundWindow() != hwnd) Sleep(100) whenever FUN_1412d6970 ("can system-pause") says
//   yes. FUN_1412d6970 is only called from there; making it return 0 removes both, and leaves the
//   window's own focus flags (cursor clipping, keyboard and mouse focus) alone.
//   Boot menu states (menu manager *DAT_144e04ce0, current state at +0x888): 0xC3 microtransaction ->
//   0xC4 epilepsy warning (8 s) -> 2 legal (3 s) -> 0x28 intro movie -> 4 press start -> 5 profile load
//   -> ... -> 8 main menu. The next-state ids are immediates in the code: 0xC4 at 0x1421e1793,
//   0x142228bc0 and 0x1421f8b09 (-> 2), 3.0f at 0x1421df9f7 (its upper half 40 40 at 0x1421df9f9 -> 0), 0x28 at 0x142227ed9 (-> 4).
//   Press start: its state's Update is vtable slot 0x142c1e490 (FUN_142228ed0); 0x142213810(state, 0)
//   is what the screen's own "PressStart" button runs (signs the user in, then requests profile load).
#define MOD_NAME L"QuickStart"
#include "..\Common\ModCommon.h"

static const uintptr_t RVA_CANPAUSE = 0x12D6970;
static const uintptr_t RVA_EPI1 = 0x21E1793, RVA_EPI2 = 0x2228BC0, RVA_EPI3 = 0x21F8B09;
static const uintptr_t RVA_LEGALWAIT = 0x21DF9F9, RVA_INTRONEXT = 0x2227ED9;   // LEGALWAIT: upper half of 3.0f
static const uintptr_t RVA_PRESSSTART_SLOT = 0x2C1E490, RVA_PRESSSTART_UPDATE = 0x2228ED0, RVA_PRESSSTART_GO = 0x2213810;
static const uintptr_t RVA_MENUMGR = 0x4E04CE0;
static const int STATE_PRESS_START = 4;

typedef void (__fastcall *UpdateFn)(uint8_t*, float);
typedef void (__fastcall *PressFn)(uint8_t*, uint8_t);

static int Setting(const wchar_t* key) {
    wchar_t path[MAX_PATH]; swprintf_s(path, L"%sQuickStart.ini", g_dir);
    return GetPrivateProfileIntW(L"QuickStart", key, 1, path);
}

// ------------------------------------------------------------------ no pause when unfocused
// FUN_1412d6970 starts at a 16-byte boundary, so its first 6 bytes lie in one aligned 8-byte block and
// are replaced in one atomic write.
static void NoFocusPause() {
    static const uint8_t orig[6] = {0x40, 0x53, 0x55, 0x57, 0x41, 0x56};   // push rbx/rbp/rdi/r14
    static const uint8_t repl[6] = {0x31, 0xC0, 0xC3, 0xCC, 0xCC, 0xCC};   // xor eax,eax; ret; int3 padding
    Log(PatchCode(RVA_CANPAUSE, orig, repl, 6) ? "focus: the game keeps running in the background"
                                               : "focus: code differs - left as it is");
}

// ------------------------------------------------------------------ startup screens
// Each patch changes the low byte(s) of a 4-byte immediate inside one aligned block.
static void SkipScreens(bool epilepsy, bool legal, bool intro) {
    static const uint8_t st_c4[4] = {0xC4, 0, 0, 0}, st_02[4] = {0x02, 0, 0, 0};
    static const uint8_t st_28[4] = {0x28, 0, 0, 0}, st_04[4] = {0x04, 0, 0, 0};
    static const uint8_t f3[2] = {0x40, 0x40}, f0[2] = {0, 0};   // 3.0f (00 00 40 40) -> 0.0f
    if (epilepsy) {
        bool ok = PatchCode(RVA_EPI1, st_c4, st_02, 4);
        ok = PatchCode(RVA_EPI2, st_c4, st_02, 4) && ok;
        ok = PatchCode(RVA_EPI3, st_c4, st_02, 4) && ok;
        Log(ok ? "startup: epilepsy warning skipped" : "startup: epilepsy warning code differs - not all skips applied");
    }
    if (legal) Log(PatchCode(RVA_LEGALWAIT, f3, f0, 2) ? "startup: legal screen wait removed" : "startup: legal screen code differs");
    if (intro) Log(PatchCode(RVA_INTRONEXT, st_28, st_04, 4) ? "startup: intro movie skipped" : "startup: intro movie code differs");
}

// ------------------------------------------------------------------ press start
// After the title screen has run a few frames, press it once (only at game start), the way its own button
// does. Later visits to the title screen (back to title from the game) are left alone.
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
    if (*slot != g_base + RVA_PRESSSTART_UPDATE) { Log("startup: press start code differs - not automated"); return; }
    g_pressUpdate = (UpdateFn)*slot;
    if (!SetPointer(slot, (void*)PressStartUpdate)) { Log("startup: could not change press start"); return; }
    Log("startup: press start will be pressed for you");
}

static void ModMain() {
    Log("Quick Start 1.0");
    if (Setting(L"NoPauseWhenUnfocused")) NoFocusPause();
    SkipScreens(Setting(L"SkipEpilepsyWarning") != 0, Setting(L"SkipLegalWait") != 0, Setting(L"SkipIntroMovie") != 0);
    if (Setting(L"AutoPressStart")) AutoPressStart();
}
