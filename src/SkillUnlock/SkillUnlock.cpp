// Skill Unlock 1.0 - an "UNLOCK ALL" tile in the Character screen's left column (under LOADOUT / SKILLS).
// by Space Trucker
// Picking it twice (UNLOCK ALL -> CONFIRM?) raises the current hero to level 50 and max champion level
// through the game's own XP code and buys every skill on every tab through the game's own purchase code.
// When nothing is left it reads ALL MAXED and does nothing. Marvel's Avengers PC, Steam 2.8.2.
//
// The column is built from a cdc::Array<int> of menu-state ids at widget+0x238 {u32 count, u32 cap,
// int* data}: 0xC7 Loadout, 0xCE Skills, 0xD2 Manifests (feature 0x1c). Three vtable slots of the widget
// (vtable 0x142c309d8) are swapped, no code is patched:
//   slot 71 Layout(w)       0x1422b3780: reads the count for AS SetNumButtons -> push our id before it
//   slot 75 Refresh(w)      0x142344520: SetState(idx, ..., title "") for unknown ids -> set our title after
//   slot 26 Action(w,a,f)   0x1422e6030: a == 0x1A1 (accept) on the selected index -> ours: run our code
#include <windows.h>
#include <stdio.h>
#include <share.h>
#include <stdint.h>

static const uint32_t PE_TIMESTAMP = 1680579845;  // 2.8.2
static const uintptr_t RVA_VTBL = 0x2C309D8;
static const uintptr_t RVA_LAYOUT = 0x22B3780, RVA_REFRESH = 0x2344520, RVA_ACTION = 0x22E6030;
static const uintptr_t RVA_SETSTATE = 0x2378D00, RVA_LOCALIZE = 0x172FC30, RVA_LOCTABLE = 0x45D9DF0;
static const uintptr_t RVA_PUSH = 0x22702E0, RVA_HERO = 0x1D8F940, RVA_SOUND = 0x1B1B9A0, RVA_SOUNDSYS = 0x4DDC3B8;
static const uintptr_t RVA_TREE = 0x1AC3E50, RVA_PROG = 0x1F74520, RVA_STATE = 0x1F7F580, RVA_BUY = 0x1F5E2D0;
static const uintptr_t RVA_ADDXP = 0x1F8CE40, RVA_XPTABLES = 0x4D97018;
static const uintptr_t RVA_TM = 0x13D7640, RVA_REMOVETRAIT = 0x1E69C90, RVA_TRAITINDEX = 0x1AC86B0;
static const uintptr_t RVA_CHANGED = 0x15D2F70, RVA_TMREFRESH = 0x1E6A9E0;
static const int SLOT_ACTION = 26, SLOT_LAYOUT = 71, SLOT_REFRESH = 75;
static const int MY_ID = 0x7F00, ACCEPT = 0x1A1;

typedef void (__fastcall *Fn1)(uint8_t*);
typedef void (__fastcall *ActionFn)(uint8_t*, int, uint8_t);
typedef void (__fastcall *SetStateFn)(uint8_t*, uint32_t, uint8_t, uint8_t, uint8_t, const char*, int, const char*);
typedef const char* (__fastcall *LocFn)(void*, const char*);
typedef void (__fastcall *PushFn)(void*, const int*);
typedef uintptr_t (__fastcall *PtrFn0)();
typedef void (__fastcall *SoundFn)(uintptr_t, const char*, uintptr_t, uintptr_t, uintptr_t, uintptr_t);
typedef uintptr_t (__fastcall *PtrFn1)(uintptr_t);
typedef uint32_t (__fastcall *StateFn)(uintptr_t, uintptr_t, uint32_t, int, uint8_t, char, uint8_t);
typedef uint8_t (__fastcall *BuyFn)(uintptr_t, uintptr_t, uint16_t, uint8_t, uint8_t);
typedef void (__fastcall *AddXpFn)(uintptr_t, int, char, uint8_t);
typedef void (__fastcall *RemoveTraitFn)(uintptr_t, uint32_t, uint8_t, const char*);
typedef uint32_t (__fastcall *TraitIndexFn)(uint32_t);
typedef void (__fastcall *ChangedFn)(uint32_t, uint8_t);

static uintptr_t g_base;
static wchar_t g_dir[MAX_PATH];
static Fn1 g_layout, g_refresh;
static ActionFn g_action;

static void Log(const char* fmt, ...) {
    wchar_t path[MAX_PATH]; swprintf_s(path, L"%sSkillUnlock.log", g_dir);
    FILE* f = _wfsopen(path, L"a", _SH_DENYNO);
    if (!f) return;
    SYSTEMTIME t; GetLocalTime(&t);
    fprintf(f, "%02d:%02d:%02d ", t.wHour, t.wMinute, t.wSecond);
    va_list a; va_start(a, fmt); vfprintf(f, fmt, a); va_end(a);
    fputc('\n', f); fclose(f);
}

static uint32_t Count(uint8_t* w) { return *(uint32_t*)(w + 0x238); }
static int* Ids(uint8_t* w) { return *(int**)(w + 0x240); }
static int MyIndex(uint8_t* w) {
    for (uint32_t i = 0; i < Count(w) && i < 16; i++) if (Ids(w)[i] == MY_ID) return (int)i;
    return -1;
}
static int Selected(uint8_t* w) {
    uint8_t* nav = *(uint8_t**)(w + 0x188);
    return nav ? *(int*)(nav + 0x84) : -1;
}

static bool g_faulted;   // after any fault the tile is no longer added and no game code is called

static void __fastcall Layout(uint8_t* w) {
    if (!g_faulted) __try {
        if (MyIndex(w) < 0 && Count(w) < 4) {   // the column has 4 tile slots; if all are used, no tile
            ((PushFn)(g_base + RVA_PUSH))(w + 0x238, &MY_ID);
            // movie name: **(w+0xC0); clip name: inline at w+0xC8 (the widget debug print at 0x1422b4ca0)
            const char* movie = *(void**)(w + 0xC0) ? **(const char***)(w + 0xC0) : nullptr;
            Log("tile added (movie '%s')", movie ? movie : "?");
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) { g_faulted = true; Log("fault adding the tile - Skill Unlock is off until the game restarts"); }
    g_layout(w);
}

// ------------------------------------------------------------------ the unlock (game thread, from the tile)
// Tree (FUN_141ac3e50): +0x1F0 tabs (0x118 each), +0x1F8 tab count. Tab: +0 enabled, +1 champion,
// +0x38 + c*0x38 four columns {+0x28 node array, +0x30 count}, node stride 0x820. Node: +8 type
// (0 single, 1/2 chain of 2/3, 3 pick-3, 4 ranked); slots {int idx at +0x80/+0x2E8/+0x550, inline def
// at +0x88/+0x2F0/+0x558}; ranked: +0x7C8 array of {int idx, pad, def at +8} stride 0x268, +0x7D0 count.
// def+0 = skill id. (static analysis of FUN_142321000 / FUN_142377a30 / FUN_142339460.)
static uint32_t XpTotal(int off) {
    uintptr_t c = *(uintptr_t*)(g_base + RVA_XPTABLES);
    if (!c || !*(uint32_t*)(c + 0x68)) return 0;
    uintptr_t e = *(uintptr_t*)(c + 0x70);
    uint32_t n = *(uint32_t*)(e + off); uintptr_t b = *(uintptr_t*)(e + off + 8);
    if (!b || n > 1000) return 0;
    uint64_t total = 0;
    for (uint32_t i = 0; i < n; i++) {
        uint8_t* band = (uint8_t*)(b + (uintptr_t)i * 0x28);
        total += (uint64_t)(uint16_t)(*(uint16_t*)(band + 6) - *(uint16_t*)(band + 4)) * *(uint32_t*)(band + 8);
    }
    return total < 0x7FFFFFFF ? (uint32_t)total : 0;
}

static uintptr_t g_hero, g_prog, g_tm;
static int g_bought;

static uint32_t State(uintptr_t def, int idx, bool champ) {
    return ((StateFn)(g_base + RVA_STATE))(g_hero, def, (uint32_t)idx, -1, 1, 0, champ ? 1 : 0);
}

static bool g_dryRun;   // count what could be bought, buy nothing

// Buy one slot if the game says it can be bought (2); 4 = only the cost is missing: buy it free.
static bool TryBuy(uintptr_t def, int idx, bool champ) {
    if (!def || idx < 0) return false;
    uint32_t st = State(def, idx, champ);
    if (st != 2 && st != 4) return false;
    if (g_dryRun) { g_bought++; return false; }
    if (!((BuyFn)(g_base + RVA_BUY))(g_hero, def, (uint16_t)idx, st == 4, champ ? 1 : 0)) return false;
    g_bought++;
    return true;
}

static bool OnePass() {
    uintptr_t tree = ((PtrFn1)(g_base + RVA_TREE))(g_hero);
    if (!tree) return false;
    uint8_t* tabs = *(uint8_t**)(tree + 0x1F0);
    uint32_t ntabs = *(uint32_t*)(tree + 0x1F8);
    bool any = false;
    for (uint32_t t = 0; tabs && t < ntabs && t < 16; t++) {
        uint8_t* tab = tabs + t * 0x118;
        if (!tab[0]) continue;
        bool champ = tab[1] != 0;
        for (int c = 0; c < 4; c++) {
            uint8_t* col = tab + 0x38 + c * 0x38;
            uint8_t* arr = *(uint8_t**)(col + 0x28);
            uint32_t n = *(uint32_t*)(col + 0x30);
            for (uint32_t i = 0; arr && i < n && i < 64; i++) {
                uint8_t* N = arr + (uintptr_t)i * 0x820;
                uint8_t type = N[8];
                if (type == 4) {
                    // ranked: buy the rank after the highest owned one, then drop the one below it, like
                    // the game's own buy button does (FUN_142339460, "Remove Prev Rank" at 0x1423396e3).
                    uint8_t* r = *(uint8_t**)(N + 0x7C8);
                    uint32_t rn = *(uint32_t*)(N + 0x7D0);
                    if (!r || !rn || rn > 16) continue;
                    int top = -1;
                    for (uint32_t k = 0; k < rn; k++)
                        if (State((uintptr_t)(r + k * 0x268 + 8), *(int*)(r + k * 0x268), champ) == 3) top = (int)k;
                    uint32_t next = (uint32_t)(top + 1);
                    if (next >= rn) continue;
                    if (TryBuy((uintptr_t)(r + next * 0x268 + 8), *(int*)(r + next * 0x268), champ)) {
                        any = true;
                        if (next > 0 && g_tm) {
                            uint32_t prevId = *(uint32_t*)(r + (next - 1) * 0x268 + 8);
                            ((RemoveTraitFn)(g_base + RVA_REMOVETRAIT))(g_tm, prevId, 1, "SkillUnlock Remove Prev Rank");
                            ((ChangedFn)(g_base + RVA_CHANGED))(((TraitIndexFn)(g_base + RVA_TRAITINDEX))(prevId), 0);
                        }
                    }
                    continue;
                }
                static const int IDX[3] = {0x80, 0x2E8, 0x550}, DEF[3] = {0x88, 0x2F0, 0x558};
                int slots = type == 0 ? 1 : type == 1 ? 2 : 3;
                for (int k = 0; k < slots; k++)
                    if (TryBuy((uintptr_t)(N + DEF[k]), *(int*)(N + IDX[k]), champ)) any = true;
            }
        }
    }
    return any;
}

static void UnlockAll() {
    g_hero = ((PtrFn0)(g_base + RVA_HERO))();
    if (!g_hero) { Log("no hero"); return; }
    g_prog = ((PtrFn1)(g_base + RVA_PROG))(g_hero);
    g_tm = ((PtrFn1)(g_base + RVA_TM))(*(uintptr_t*)(g_hero + 0x3490));
    if (!g_prog) { Log("no progression"); return; }
    g_bought = 0;
    // 1. level 50 and max champion level through the game's AddXP (levels, points, rewards)
    for (int step = 0; step < 2; step++) {
        uint32_t total = XpTotal(step == 0 ? 0x28 : 0x140);
        uint32_t have = *(uint32_t*)(g_prog + (step == 0 ? 0x58 : 0x5C));
        if (total && have < total && *(uint8_t*)(g_prog + 0x54)) {
            ((AddXpFn)(g_base + RVA_ADDXP))(g_prog, (int)(total - have), step == 1 ? 1 : 0, 1);
            Log("  %s XP %u -> %u (level %u, champion level %u)", step ? "champion" : "hero", have, total,
                *(uint32_t*)(g_prog + 0x60) + 1, *(uint32_t*)(g_prog + 0x64));
        }
    }
    // 2. every node on every tab, repeated until nothing more can be bought (prerequisite chains).
    // Points are kept: whatever could not be paid was bought free (state 4), so nothing is spent.
    uint32_t sp = *(uint32_t*)(g_prog + 0x74), cp = *(uint32_t*)(g_prog + 0x7C);
    int passes = 1;
    while (passes < 12 && OnePass()) passes++;
    *(uint32_t*)(g_prog + 0x74) = sp;
    *(uint32_t*)(g_prog + 0x7C) = cp;
    if (g_tm) ((Fn1)(g_base + RVA_TMREFRESH))((uint8_t*)g_tm);
    Log("done: bought %d skill(s) in %d pass(es); %u skill points and %u champion points kept", g_bought, passes, sp, cp);
    ((SoundFn)(g_base + RVA_SOUND))(*(uintptr_t*)(g_base + RVA_SOUNDSYS), "UI_Skills_Pick3SkillPurchased",
                                    *(uintptr_t*)(g_hero + 0x3490), 0, 0, 0);
}

// Nothing left to do for the current hero: max level, max champion level, no skill that can be bought.
// Checked against the live tree each time the tile is drawn (a dry pass, nothing is bought).
static bool AllDone() {
    g_hero = ((PtrFn0)(g_base + RVA_HERO))();
    if (!g_hero) return false;
    g_prog = ((PtrFn1)(g_base + RVA_PROG))(g_hero);
    if (!g_prog) return false;
    uint32_t hx = XpTotal(0x28), cx = XpTotal(0x140);
    if ((hx && *(uint32_t*)(g_prog + 0x58) < hx) || (cx && *(uint32_t*)(g_prog + 0x5C) < cx)) return false;
    g_dryRun = true; g_bought = 0;
    OnePass();
    g_dryRun = false;
    return g_bought == 0;
}

// Tile text: the Flash text field shows one line of about 10 characters ("\n" is ignored, tested), and
// the tile's look (no hero header, up-arrow icon) is decided inside the Flash movie, not by any call.
// normal -> CONFIRM? (4 s) -> ALL MAXED
static DWORD g_armedAt, g_lastPress;

static const char* Title(bool done) {
    if (done) return "ALL MAXED";
    if (g_armedAt && GetTickCount() - g_armedAt < 4000) return "CONFIRM?";
    return "UNLOCK ALL";
}

// AllDone walks the whole tree, so the tile's redraws reuse the answer for a moment.
static bool AllDoneCached() {
    static DWORD at; static uintptr_t hero; static bool done;
    uintptr_t h = ((PtrFn0)(g_base + RVA_HERO))();
    if (h != hero || GetTickCount() - at > 500) { done = AllDone(); hero = h; at = GetTickCount(); }
    return done;
}

static void __fastcall Refresh(uint8_t* w) {
    g_refresh(w);
    if (g_faulted) return;
    __try {
        int i = MyIndex(w);
        if (i < 0 || !*(void**)(w + 0xB8)) return;   // no Flash movie: nothing to draw
        const char* sub = ((LocFn)(g_base + RVA_LOCALIZE))(*(void**)(g_base + RVA_LOCTABLE), "BL_Select");
        bool done = AllDoneCached();
        ((SetStateFn)(g_base + RVA_SETSTATE))(w, (uint32_t)i, 0, Selected(w) == i, 0, Title(done), done ? 0 : ACCEPT, sub);
    } __except (EXCEPTION_EXECUTE_HANDLER) { g_faulted = true; Log("fault drawing the tile - Skill Unlock is off until the game restarts"); }
}

static void __fastcall Action(uint8_t* w, int action, uint8_t flag) {
    bool mine = false;
    __try { mine = action == ACCEPT && MyIndex(w) >= 0 && Selected(w) == MyIndex(w); }
    __except (EXCEPTION_EXECUTE_HANDLER) {}
    if (!mine || g_faulted) { g_action(w, action, flag); return; }
    DWORD now = GetTickCount();
    if (now - g_lastPress < 400) return;   // a mouse click arrives twice
    g_lastPress = now;
    bool done = false;
    __try { done = AllDone(); } __except (EXCEPTION_EXECUTE_HANDLER) { g_faulted = true; return; }
    if (done) { g_armedAt = 0; return; }   // nothing left: the tile does nothing
    if (!g_armedAt || now - g_armedAt >= 4000) {
        g_armedAt = now;
        Log("UNLOCK ALL SKILLS: waiting for confirmation");
    } else {
        g_armedAt = 0;
        Log("UNLOCK ALL SKILLS confirmed");
        __try { UnlockAll(); }
        __except (EXCEPTION_EXECUTE_HANDLER) { g_faulted = true; Log("fault during unlock - Skill Unlock is off until the game restarts"); return; }
    }
    __try { Refresh(w); } __except (EXCEPTION_EXECUTE_HANDLER) {}
}

// A newer build loaded over a running one finds its functions in the vtable: take over from it.
static bool OursOrOriginal(uintptr_t p, uintptr_t expect) {
    if (p == g_base + expect) return true;
    HMODULE m = nullptr; wchar_t name[MAX_PATH] = L"";
    return GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                              (LPCWSTR)p, &m) && GetModuleFileNameW(m, name, MAX_PATH) &&
           (wcsstr(name, L"SkillTile") || wcsstr(name, L"SkillUnlock"));
}

// Writes one vtable entry. The page keeps its execute right if it had one; the store is an aligned
// 8-byte exchange, so other threads see either the old or the new pointer.
static bool SetSlot(int slot, void* value) {
    uintptr_t* e = (uintptr_t*)(g_base + RVA_VTBL) + slot;
    MEMORY_BASIC_INFORMATION mbi;
    if (!VirtualQuery(e, &mbi, sizeof(mbi))) return false;
    DWORD exec = PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY;
    DWORD old;
    if (!VirtualProtect(e, 8, (mbi.Protect & exec) ? PAGE_EXECUTE_READWRITE : PAGE_READWRITE, &old)) return false;
    InterlockedExchangePointer((void**)e, value);
    VirtualProtect(e, 8, old, &old);
    return true;
}

static DWORD WINAPI Main(LPVOID) {
    g_base = (uintptr_t)GetModuleHandleW(nullptr);
    auto nt = (IMAGE_NT_HEADERS*)(g_base + ((IMAGE_DOS_HEADER*)g_base)->e_lfanew);
    if (nt->FileHeader.TimeDateStamp != PE_TIMESTAMP) { Log("wrong game version (needs Steam 2.8.2)"); return 0; }
    // check all three first, so it is all or nothing
    uintptr_t* v = (uintptr_t*)(g_base + RVA_VTBL);
    if (!OursOrOriginal(v[SLOT_LAYOUT], RVA_LAYOUT) || !OursOrOriginal(v[SLOT_REFRESH], RVA_REFRESH) ||
        !OursOrOriginal(v[SLOT_ACTION], RVA_ACTION)) {
        Log("character screen code differs (another mod?) - not installed"); return 0;
    }
    // always chain to the game's own functions
    g_layout = (Fn1)(g_base + RVA_LAYOUT);
    g_refresh = (Fn1)(g_base + RVA_REFRESH);
    g_action = (ActionFn)(g_base + RVA_ACTION);
    if (!SetSlot(SLOT_LAYOUT, (void*)Layout) || !SetSlot(SLOT_REFRESH, (void*)Refresh) || !SetSlot(SLOT_ACTION, (void*)Action)) {
        SetSlot(SLOT_LAYOUT, (void*)g_layout); SetSlot(SLOT_REFRESH, (void*)g_refresh); SetSlot(SLOT_ACTION, (void*)g_action);
        Log("could not change the character screen safely - not installed");
        return 0;
    }
    Log("Skill Unlock 1.0 ready: Character screen > UNLOCK ALL (press twice to confirm)");
    return 0;
}

extern "C" __declspec(dllexport) int AvengersModInit(void*) { return 1; }

BOOL WINAPI DllMain(HINSTANCE h, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(h);
        GetModuleFileNameW(h, g_dir, MAX_PATH);
        wchar_t* s = wcsrchr(g_dir, L'\\'); if (s) s[1] = 0;
        HANDLE t = CreateThread(nullptr, 0, Main, nullptr, 0, nullptr);
        if (t) CloseHandle(t);
    }
    return TRUE;
}
