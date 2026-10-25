#define MOD_NAME L"SkillUnlock"
#include "..\Common\CharacterTiles.h"

using namespace tiles;

static const uintptr_t RVA_TREE = 0x1AC3E50, RVA_PROG = 0x1F74520, RVA_STATE = 0x1F7F580, RVA_BUY = 0x1F5E2D0;
static const uintptr_t RVA_ADDXP = 0x1F8CE40, RVA_XPTABLES = 0x4D97018;
static const uintptr_t RVA_TM = 0x13D7640, RVA_REMOVETRAIT = 0x1E69C90, RVA_TRAITINDEX = 0x1AC86B0;
static const uintptr_t RVA_CHANGED = 0x15D2F70, RVA_TMREFRESH = 0x1E6A9E0;
static const int MAX_PASSES = 12;

enum { NODE_SINGLE = 0, NODE_CHAIN2 = 1, NODE_CHAIN3 = 2, NODE_PICK3 = 3, NODE_RANKED = 4 };

typedef uintptr_t (__fastcall *PtrFn1)(uintptr_t);
typedef uint32_t (__fastcall *StateFn)(uintptr_t, uintptr_t, uint32_t, int, uint8_t, char, uint8_t);
typedef uint8_t (__fastcall *BuyFn)(uintptr_t, uintptr_t, uint16_t, uint8_t, uint8_t);
typedef void (__fastcall *AddXpFn)(uintptr_t, int, char, uint8_t);
typedef void (__fastcall *RemoveTraitFn)(uintptr_t, uint32_t, uint8_t, const char*);
typedef uint32_t (__fastcall *TraitIndexFn)(uint32_t);
typedef void (__fastcall *ChangedFn)(uint32_t, uint8_t);

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

static bool BindHero() {
    g_hero = PlayerEntity();
    if (!g_hero) return false;
    g_prog = ((PtrFn1)(g_base + RVA_PROG))(g_hero);
    uintptr_t part = InventoryPart(g_hero);
    g_tm = part ? ((PtrFn1)(g_base + RVA_TM))(part) : 0;
    return g_prog != 0;
}

static uint32_t State(uintptr_t def, int idx, bool champ) {
    return ((StateFn)(g_base + RVA_STATE))(g_hero, def, (uint32_t)idx, -1, 1, 0, champ ? 1 : 0);
}

static bool g_dryRun;

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
                if (type > NODE_RANKED) {
                    static bool logged;
                    if (!logged) { logged = true; Log("a skill node of unknown type %u - skipped", type); }
                    continue;
                }
                if (type == NODE_RANKED) {
                    if (!g_tm) {
                        static bool logged;
                        if (!logged) { logged = true; Log("no trait manager for this hero - ranked skills skipped"); }
                        continue;
                    }
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
                        if (next > 0) {
                            uint32_t prevId = *(uint32_t*)(r + (next - 1) * 0x268 + 8);
                            ((RemoveTraitFn)(g_base + RVA_REMOVETRAIT))(g_tm, prevId, 1, "SkillUnlock Remove Prev Rank");
                            ((ChangedFn)(g_base + RVA_CHANGED))(((TraitIndexFn)(g_base + RVA_TRAITINDEX))(prevId), 0);
                        }
                    }
                    continue;
                }
                static const int IDX[3] = {0x80, 0x2E8, 0x550}, DEF[3] = {0x88, 0x2F0, 0x558};
                int slots = type == NODE_SINGLE ? 1 : type == NODE_CHAIN2 ? 2 : 3;
                for (int k = 0; k < slots; k++)
                    if (TryBuy((uintptr_t)(N + DEF[k]), *(int*)(N + IDX[k]), champ)) any = true;
            }
        }
    }
    return any;
}

static int BuyAll(uint32_t sp, uint32_t cp) {
    int passes = 0;
    __try {
        while (passes < MAX_PASSES) { passes++; if (!OnePass()) break; }
    } __finally {
        __try {
            *(uint32_t*)(g_prog + 0x74) = sp;
            *(uint32_t*)(g_prog + 0x7C) = cp;
        } FAULT {}
    }
    return passes;
}

static void UnlockAll() {
    if (!BindHero()) { Log(g_hero ? "no progression" : "no hero"); return; }
    g_bought = 0;

    for (int step = 0; step < 2; step++) {
        uint32_t total = XpTotal(step == 0 ? 0x28 : 0x140);
        uint32_t have = *(uint32_t*)(g_prog + (step == 0 ? 0x58 : 0x5C));
        if (total && have < total && *(uint8_t*)(g_prog + 0x54)) {
            ((AddXpFn)(g_base + RVA_ADDXP))(g_prog, (int)(total - have), step == 1 ? 1 : 0, 1);
            Log("  %s XP %u -> %u (level %u, champion level %u)", step ? "champion" : "hero", have, total,
                *(uint32_t*)(g_prog + 0x60) + 1, *(uint32_t*)(g_prog + 0x64));
        }
    }

    uint32_t sp = *(uint32_t*)(g_prog + 0x74), cp = *(uint32_t*)(g_prog + 0x7C);
    int passes = BuyAll(sp, cp);
    if (g_tm) ((Fn1)(g_base + RVA_TMREFRESH))((uint8_t*)g_tm);
    Log("done: bought %d skill(s) in %d pass(es); %u skill points and %u champion points kept", g_bought, passes, sp, cp);
    PlayUnlockSound(g_hero);
}

static bool AllDone() {
    if (!BindHero()) return false;
    uint32_t hx = XpTotal(0x28), cx = XpTotal(0x140);
    if ((hx && *(uint32_t*)(g_prog + 0x58) < hx) || (cx && *(uint32_t*)(g_prog + 0x5C) < cx)) return false;
    g_dryRun = true; g_bought = 0;
    OnePass();
    g_dryRun = false;
    return g_bought == 0;
}

static TileHooks g_next;
static TileButton g_tile = { SKILL_TILE_ID, "UNLOCK ALL", "ALL MAXED", "UNLOCK ALL SKILLS", AllDone, UnlockAll };

static void __fastcall Layout(uint8_t* w) {
    if (!g_tile.faulted) __try {
        if (IndexOf(w, g_tile.id) < 0 && Count(w) < (uint32_t)COLUMN_SLOTS) {
            PushTile(w, &g_tile.id);
            static bool logged;
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
    TileLock lock;
    if (!lock.ok()) { Log("could not take the install lock - not installed"); return; }
    SlotOwnerKind under;
    if (const char* err = InstallTile(OWNER_SKILL, Layout, Refresh, Action, g_next, under)) {
        Log("%s - not installed", err); return;
    }
    Log("Skill Unlock %s ready: Character screen > UNLOCK ALL (press twice to confirm)%s", MOD_VERSION,
        under == OWNER_COLLECTION ? ", together with Collection Unlock" : under == OWNER_CHAIN ? ", over another tile mod" : "");
}
