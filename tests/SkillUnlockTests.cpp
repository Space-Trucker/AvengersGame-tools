#define MOD_VERSION "test"
#include "../src/SkillUnlock/SkillUnlock.cpp"
#include "Check.h"
#include "FakeImage.h"
#include <initializer_list>

static wchar_t g_tmp[MAX_PATH];

static const char* LogText() {
    static char text[1 << 16];
    text[0] = 0;
    wchar_t path[MAX_PATH];
    _snwprintf_s(path, _countof(path), _TRUNCATE, L"%sSkillUnlock.log", g_tmp);
    FILE* f = nullptr;
    if (_wfopen_s(&f, path, L"rb") || !f) return text;
    text[fread(text, 1, sizeof text - 1, f)] = 0;
    fclose(f);
    return text;
}
static void ClearLog() {
    wchar_t path[MAX_PATH];
    _snwprintf_s(path, _countof(path), _TRUNCATE, L"%sSkillUnlock.log", g_tmp);
    DeleteFileW(path);
}
static bool Logged(const char* text) { return strstr(LogText(), text) != nullptr; }

static uint8_t g_entity[0x4000], g_part[0x10], g_progBuf[0x100], g_tmBuf[0x10];
static uint8_t g_xpc[0x80], g_xpe[0x160], g_heroBands[0x28], g_champBands[0x28];
static uint8_t g_tree[0x200], g_tabs[3 * 0x118];
static uint8_t g_nodes0[6 * 0x820], g_nodes1[0x820], g_nodes2[0x820], g_ranks[3 * 0x268];
static bool g_noHero, g_noProg, g_tmOn;
static int g_buys, g_faultOnBuy, g_refreshes, g_sounds;
static uint32_t g_removed[8], g_changed[8];
static int g_removedCount, g_changedCount;
static uint32_t g_boughtIds[16];

static uintptr_t __fastcall FakePlayer() { return g_noHero ? 0 : (uintptr_t)g_entity; }
static uintptr_t __fastcall FakeProg(uintptr_t) { return g_noProg ? 0 : (uintptr_t)g_progBuf; }
static uintptr_t __fastcall FakeTm(uintptr_t) { return g_tmOn ? (uintptr_t)g_tmBuf : 0; }
static uintptr_t __fastcall FakeTree(uintptr_t) { return (uintptr_t)g_tree; }
static uint32_t __fastcall FakeState(uintptr_t, uintptr_t def, uint32_t, int, uint8_t, char, uint8_t) { return *(uint32_t*)(def + 4); }
static uint8_t __fastcall FakeBuy(uintptr_t, uintptr_t def, uint16_t, uint8_t free, uint8_t champ) {
    if (++g_buys == g_faultOnBuy) *(volatile int*)nullptr = 1;
    if (!free) (*(uint32_t*)(g_progBuf + (champ ? 0x7C : 0x74)))--;
    *(uint32_t*)(def + 4) = 3;
    uint8_t* unlocks = *(uint8_t**)(def + 8);
    if (unlocks) *(uint32_t*)(unlocks + 4) = 2;
    if (g_buys <= 16) g_boughtIds[g_buys - 1] = *(uint32_t*)def;
    return 1;
}
static void __fastcall FakeAddXp(uintptr_t prog, int amount, char champ, uint8_t) {
    *(uint32_t*)(prog + (champ ? 0x5C : 0x58)) += (uint32_t)amount;
    if (champ) *(uint32_t*)(prog + 0x64) = 10; else *(uint32_t*)(prog + 0x60) = 49;
}
static void __fastcall FakeRemoveTrait(uintptr_t, uint32_t id, uint8_t, const char*) { if (g_removedCount < 8) g_removed[g_removedCount++] = id; }
static uint32_t __fastcall FakeTraitIndex(uint32_t id) { return id + 1000; }
static void __fastcall FakeChanged(uint32_t index, uint8_t) { if (g_changedCount < 8) g_changed[g_changedCount++] = index; }
static void __fastcall FakeTmRefresh(uint8_t*) { g_refreshes++; }
static void __fastcall FakeSound(uintptr_t, const char*, uintptr_t, uintptr_t, uintptr_t, uintptr_t) { g_sounds++; }

static uint8_t* Slot(uint8_t* node, int k, uint32_t id, uint32_t state, int idx = 0) {
    static const int IDX_AT[3] = { 0x80, 0x2E8, 0x550 }, DEF_AT[3] = { 0x88, 0x2F0, 0x558 };
    *(int*)(node + IDX_AT[k]) = idx;
    uint8_t* def = node + DEF_AT[k];
    *(uint32_t*)def = id;
    *(uint32_t*)(def + 4) = state;
    return def;
}
static uint8_t* Rank(int k, uint32_t id, uint32_t state) {
    uint8_t* r = g_ranks + k * 0x268;
    *(int*)r = k;
    *(uint32_t*)(r + 8) = id;
    *(uint32_t*)(r + 12) = state;
    return r + 8;
}
static void Unlocks(uint8_t* def, uint8_t* next) { *(uint8_t**)(def + 8) = next; }
static void Column(uint8_t* tab, uint8_t* nodes, uint32_t n) { *(uint8_t**)(tab + 0x38 + 0x28) = nodes; *(uint32_t*)(tab + 0x38 + 0x30) = n; }

static const uint32_t SP = 7, CP = 3;
static const size_t IMAGE_SIZE = 0x4E10000;

static void NewHero(FakeImage& im) {
    memset(g_entity, 0, sizeof g_entity);
    *(uintptr_t*)(g_entity + ENTITY_INVENTORY_PART) = (uintptr_t)g_part;
    memset(g_progBuf, 0, sizeof g_progBuf);
    g_progBuf[0x54] = 1;
    *(uint32_t*)(g_progBuf + 0x74) = SP;
    *(uint32_t*)(g_progBuf + 0x7C) = CP;
    memset(g_xpc, 0, sizeof g_xpc); memset(g_xpe, 0, sizeof g_xpe);
    *(uint32_t*)(g_xpc + 0x68) = 1; *(uint8_t**)(g_xpc + 0x70) = g_xpe;
    *(uint32_t*)(g_xpe + 0x28) = 1; *(uint8_t**)(g_xpe + 0x30) = g_heroBands;
    *(uint32_t*)(g_xpe + 0x140) = 1; *(uint8_t**)(g_xpe + 0x148) = g_champBands;
    *(uint16_t*)(g_heroBands + 4) = 1; *(uint16_t*)(g_heroBands + 6) = 50; *(uint32_t*)(g_heroBands + 8) = 1000;
    *(uint16_t*)(g_champBands + 4) = 0; *(uint16_t*)(g_champBands + 6) = 10; *(uint32_t*)(g_champBands + 8) = 500;
    memset(g_tree, 0, sizeof g_tree); memset(g_tabs, 0, sizeof g_tabs);
    memset(g_nodes0, 0, sizeof g_nodes0); memset(g_nodes1, 0, sizeof g_nodes1); memset(g_nodes2, 0, sizeof g_nodes2);
    memset(g_ranks, 0, sizeof g_ranks);
    *(uint8_t**)(g_tree + 0x1F0) = g_tabs; *(uint32_t*)(g_tree + 0x1F8) = 3;
    uint8_t* tab0 = g_tabs;
    tab0[0] = 1;
    Column(tab0, g_nodes0, 6);
    uint8_t* n = g_nodes0;
    n[8] = NODE_SINGLE; Slot(n, 0, 101, 2);
    n += 0x820; n[8] = NODE_CHAIN2; Unlocks(Slot(n, 0, 111, 2), Slot(n, 1, 112, 0, 1));
    n += 0x820; n[8] = NODE_PICK3; Slot(n, 0, 121, 2); Slot(n, 1, 122, 4, 1); Slot(n, 2, 123, 1, 2);
    n += 0x820; n[8] = NODE_RANKED;
    *(uint8_t**)(n + 0x7C8) = g_ranks; *(uint32_t*)(n + 0x7D0) = 3;
    Rank(0, 131, 3); Unlocks(Rank(1, 132, 2), Rank(2, 133, 0));
    n += 0x820; n[8] = 5; Slot(n, 0, 141, 2);
    n += 0x820; n[8] = NODE_SINGLE; Slot(n, 0, 151, 2, -1);
    uint8_t* tab1 = g_tabs + 0x118;
    tab1[0] = 1; tab1[1] = 1;
    Column(tab1, g_nodes1, 1);
    g_nodes1[8] = NODE_SINGLE; Slot(g_nodes1, 0, 201, 2);
    uint8_t* tab2 = g_tabs + 2 * 0x118;
    Column(tab2, g_nodes2, 1);
    g_nodes2[8] = NODE_SINGLE; Slot(g_nodes2, 0, 301, 2);
    g_noHero = g_noProg = false; g_tmOn = true;
    g_buys = g_faultOnBuy = g_refreshes = g_sounds = g_removedCount = g_changedCount = 0;
    memset(g_boughtIds, 0, sizeof g_boughtIds);
    memset(&g_tile, 0, sizeof g_tile);
    g_tile = { SKILL_TILE_ID, "UNLOCK ALL", "ALL MAXED", "UNLOCK ALL SKILLS", AllDone, UnlockAll };
    const struct { uintptr_t rva; const void* fn; } code[] = {
        { RVA_HERO, (const void*)FakePlayer }, { RVA_PROG, (const void*)FakeProg }, { RVA_TM, (const void*)FakeTm },
        { RVA_TREE, (const void*)FakeTree }, { RVA_STATE, (const void*)FakeState }, { RVA_BUY, (const void*)FakeBuy },
        { RVA_ADDXP, (const void*)FakeAddXp }, { RVA_REMOVETRAIT, (const void*)FakeRemoveTrait },
        { RVA_TRAITINDEX, (const void*)FakeTraitIndex }, { RVA_CHANGED, (const void*)FakeChanged },
        { RVA_TMREFRESH, (const void*)FakeTmRefresh }, { RVA_SOUND, (const void*)FakeSound } };
    for (const auto& c : code) CHECK(im.Jump(c.rva, c.fn));
    const uint8_t* xp = g_xpc;
    CHECK(im.Put(RVA_XPTABLES, &xp, 8, PAGE_READWRITE));
    CHECK(im.Put(RVA_SOUNDSYS, &xp, 8, PAGE_READWRITE));
    g_base = (uintptr_t)im.base;
    ClearLog();
}

static bool Bought(uint32_t id) {
    for (int i = 0; i < g_buys && i < 16; i++) if (g_boughtIds[i] == id) return true;
    return false;
}

static void TestUnlockAll() {
    FakeImage im(IMAGE_SIZE);
    NewHero(im);
    CHECK(!AllDone());
    UnlockAll();
    CHECK_EQ(*(uint32_t*)(g_progBuf + 0x58), 49000u);
    CHECK_EQ(*(uint32_t*)(g_progBuf + 0x5C), 5000u);
    for (uint32_t id : { 101u, 111u, 112u, 121u, 122u, 132u, 133u, 201u }) CHECK(Bought(id));
    for (uint32_t id : { 123u, 141u, 151u, 301u }) CHECK(!Bought(id));
    CHECK_EQ(g_buys, 8);
    CHECK_EQ(g_removedCount, 2);
    CHECK(g_removed[0] == 131 && g_removed[1] == 132);
    CHECK(g_changed[0] == 1131 && g_changed[1] == 1132);
    CHECK_EQ(g_refreshes, 1);
    CHECK_EQ(*(uint32_t*)(g_progBuf + 0x74), SP);
    CHECK_EQ(*(uint32_t*)(g_progBuf + 0x7C), CP);
    CHECK(Logged("done: bought 8 skill(s) in 3 pass(es); 7 skill points and 3 champion points kept"));
    CHECK(Logged("a skill node of unknown type 5 - skipped"));
    CHECK_EQ(g_sounds, 1);
    CHECK(AllDone());
    CHECK_EQ(g_buys, 8);
}

static void TestNoTraitManager() {
    FakeImage im(IMAGE_SIZE);
    NewHero(im);
    g_tmOn = false;
    UnlockAll();
    CHECK(!Bought(132) && !Bought(133));
    CHECK(Bought(101) && Bought(112) && Bought(201));
    CHECK_EQ(g_removedCount, 0);
    CHECK_EQ(g_refreshes, 0);
    CHECK(Logged("no trait manager for this hero - ranked skills skipped"));
    *(uintptr_t*)(g_entity + ENTITY_INVENTORY_PART) = 0;
    g_tmOn = true;
    *(uint32_t*)(g_ranks + 0x268 + 12) = 2;
    UnlockAll();
    CHECK(!Bought(132));
}

static void TestNoHero() {
    FakeImage im(IMAGE_SIZE);
    NewHero(im);
    g_noHero = true;
    UnlockAll();
    CHECK(Logged("no hero"));
    CHECK(!AllDone());
    g_noHero = false; g_noProg = true;
    ClearLog();
    UnlockAll();
    CHECK(Logged("no progression"));
    CHECK_EQ(g_buys, 0);
}

static uint8_t g_column[0x300];
static int g_columnIds[4];
static uint8_t g_nav[0x100];
static void* g_columnVtbl[80];
static void __fastcall FakeColumnRefresh(uint8_t*) {}

static void TestFaultMidRun() {
    FakeImage im(IMAGE_SIZE);
    NewHero(im);
    memset(g_column, 0, sizeof g_column);
    g_columnVtbl[SLOT_REFRESH] = (void*)FakeColumnRefresh;
    *(void**)g_column = g_columnVtbl;
    g_columnIds[0] = 0xC7; g_columnIds[1] = 0xCE; g_columnIds[2] = SKILL_TILE_ID;
    *(uint32_t*)(g_column + 0x238) = 3;
    *(int**)(g_column + 0x240) = g_columnIds;
    *(uint8_t**)(g_column + 0x188) = g_nav;
    *(int*)(g_nav + 0x84) = 2;
    g_faultOnBuy = 3;
    CHECK(TileAction(g_tile, g_column, ACCEPT));
    Sleep(DOUBLE_CLICK_MS + 50);
    CHECK(TileAction(g_tile, g_column, ACCEPT));
    CHECK(g_tile.faulted);
    CHECK_EQ(g_buys, 3);
    CHECK(*(uint32_t*)(g_progBuf + 0x74) == SP && *(uint32_t*)(g_progBuf + 0x7C) == CP);
    CHECK(Logged("fault during UNLOCK ALL SKILLS - it stopped part-way and is off until the game restarts; restart the game before playing on"));
    Sleep(DOUBLE_CLICK_MS + 50);
    CHECK(TileAction(g_tile, g_column, ACCEPT));
    CHECK_EQ(g_buys, 3);
}

int main() {
    wchar_t base[MAX_PATH];
    GetTempPathW(MAX_PATH, base);
    _snwprintf_s(g_tmp, _countof(g_tmp), _TRUNCATE, L"%sSkillUnlockTests-%lu\\", base, GetCurrentProcessId());
    CreateDirectoryW(g_tmp, nullptr);
    wcscpy_s(g_dir, g_tmp);
    TestUnlockAll();
    TestNoTraitManager();
    TestNoHero();
    TestFaultMidRun();
    ClearLog();
    RemoveDirectoryW(g_tmp);
    return TestSummary("SkillUnlockTests");
}
