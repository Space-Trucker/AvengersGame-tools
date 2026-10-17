// Skill Unlock 1.1 - an "UNLOCK ALL" tile in the Character screen's left column (under LOADOUT / SKILLS).
// by Space Trucker
// Picking it twice (UNLOCK ALL -> CONFIRM?) raises the current hero to level 50 and max champion level
// through the game's own XP code and buys every skill on every tab through the game's own purchase code.
// When nothing is left it reads ALL MAXED and does nothing. Marvel's Avengers PC, Steam 2.8.2.
// The tile and how it shares the column with Collection Unlock: see ..\Common\CharacterTiles.h.
#define MOD_NAME L"SkillUnlock"
#include "..\Common\CharacterTiles.h"

static const uintptr_t RVA_TREE = 0x1AC3E50, RVA_PROG = 0x1F74520, RVA_STATE = 0x1F7F580, RVA_BUY = 0x1F5E2D0;
static const uintptr_t RVA_ADDXP = 0x1F8CE40, RVA_XPTABLES = 0x4D97018;
static const uintptr_t RVA_TM = 0x13D7640, RVA_REMOVETRAIT = 0x1E69C90, RVA_TRAITINDEX = 0x1AC86B0;
static const uintptr_t RVA_CHANGED = 0x15D2F70, RVA_TMREFRESH = 0x1E6A9E0;

typedef uintptr_t (__fastcall *PtrFn1)(uintptr_t);
typedef uint32_t (__fastcall *StateFn)(uintptr_t, uintptr_t, uint32_t, int, uint8_t, char, uint8_t);
typedef uint8_t (__fastcall *BuyFn)(uintptr_t, uintptr_t, uint16_t, uint8_t, uint8_t);
typedef void (__fastcall *AddXpFn)(uintptr_t, int, char, uint8_t);
typedef void (__fastcall *RemoveTraitFn)(uintptr_t, uint32_t, uint8_t, const char*);
typedef uint32_t (__fastcall *TraitIndexFn)(uint32_t);
typedef void (__fastcall *ChangedFn)(uint32_t, uint8_t);

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
    if (!e) return 0;
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
    g_hero = PlayerEntity();
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
    int passes = 0;
    while (passes < 12) { passes++; if (!OnePass()) break; }
    *(uint32_t*)(g_prog + 0x74) = sp;
    *(uint32_t*)(g_prog + 0x7C) = cp;
    if (g_tm) ((Fn1)(g_base + RVA_TMREFRESH))((uint8_t*)g_tm);
    Log("done: bought %d skill(s) in %d pass(es); %u skill points and %u champion points kept", g_bought, passes, sp, cp);
    PlayUnlockSound(g_hero);
}

// Nothing left to do for the current hero: max level, max champion level, no skill that can be bought.
// Checked against the live tree each time the tile is drawn (a dry pass, nothing is bought).
static bool AllDone() {
    g_hero = PlayerEntity();
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

static TileHooks g_next;   // what the slots held before us: the game's functions or Collection Unlock's
static TileButton g_tile = { SKILL_TILE_ID, "UNLOCK ALL", "ALL MAXED", "UNLOCK ALL SKILLS", AllDone, UnlockAll };

static void __fastcall Layout(uint8_t* w) {
    if (!g_tile.faulted) __try {
        if (IndexOf(w, g_tile.id) < 0 && Count(w) < (uint32_t)COLUMN_SLOTS) {   // no free slot: no tile
            PushTile(w, &g_tile.id);
            static bool logged;   // the column is rebuilt every time the screen opens: log it once
            if (!logged) { logged = true; Log("tile added"); }
        }
    } FAULT { g_tile.faulted = true; Log("fault adding the tile - Skill Unlock is off until the game restarts"); }
    g_next.layout(w);
}

static void __fastcall Refresh(uint8_t* w) { g_next.refresh(w); TileRefresh(g_tile, w); }
static void __fastcall Action(uint8_t* w, int action, uint8_t flag) {
    if (!TileAction(g_tile, w, action)) g_next.action(w, action, flag);
}

static void ModMain() {
    TileLock lock;   // one tile mod installs at a time, so each wraps the other's finished hooks
    if (!lock.ok()) { Log("could not take the install lock - not installed"); return; }
    SlotOwnerKind under;
    if (const char* err = InstallTile(OWNER_SKILL, Layout, Refresh, Action, g_next, under)) {
        Log("%s - not installed", err); return;
    }
    Log("Skill Unlock 1.1 ready: Character screen > UNLOCK ALL (press twice to confirm)%s",
        under == OWNER_COLLECTION ? ", together with Collection Unlock" : "");
}
