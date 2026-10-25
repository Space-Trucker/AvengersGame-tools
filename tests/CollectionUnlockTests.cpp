#define MOD_VERSION "test"
#include "../src/CollectionUnlock/CollectionUnlock.cpp"
#include "Check.h"
#include "FakeImage.h"

static wchar_t g_tmp[MAX_PATH];

static const char* LogText() {
    static char text[1 << 16];
    text[0] = 0;
    wchar_t path[MAX_PATH];
    _snwprintf_s(path, _countof(path), _TRUNCATE, L"%sCollectionUnlock.log", g_tmp);
    FILE* f = nullptr;
    if (_wfopen_s(&f, path, L"rb") || !f) return text;
    text[fread(text, 1, sizeof text - 1, f)] = 0;
    fclose(f);
    return text;
}
static void ClearLog() {
    wchar_t path[MAX_PATH];
    _snwprintf_s(path, _countof(path), _TRUNCATE, L"%sCollectionUnlock.log", g_tmp);
    DeleteFileW(path);
}
static bool Logged(const char* text) { return strstr(LogText(), text) != nullptr; }

struct Cosmetic { uint32_t id, type; bool owned, unavailable, refused; uint32_t unlocks; int gives; };
static Cosmetic g_items[16];
static int g_itemCount;
static uint8_t g_list[16 * 0x18];
static uint8_t g_lockHdr[16];
static uint8_t g_entity[0x4000], g_profile[0x800], g_profile2[0x800], g_account[0x500C + 0x400 * 8];
static uint8_t g_db[0x420], g_set[0xA8], g_colItems[3 * 0x150], g_tables[0x18], g_tiers[2 * 0x18];
static uint8_t g_giftCfg[0x48], g_cat[0x10], g_outfit[0x160], g_styles[0x10], g_heroes[0x18], g_heroArray[0x248];
static uint8_t g_hero[0x10], g_slot[0x30];
static uintptr_t g_currentProfile;
static bool g_heroExists, g_slotExists;
static int g_colGives, g_sounds;
static bool g_giveReturns = true;

static Cosmetic* Find(uint32_t id) {
    for (int i = 0; i < g_itemCount; i++) if (g_items[i].id == id) return &g_items[i];
    return nullptr;
}
static void Show(uint32_t id) {
    uint32_t n = *(uint32_t*)(g_lockHdr + 8);
    *(uint32_t*)(g_list + (uintptr_t)n * 0x18 + 4) = id;
    *(uint32_t*)(g_lockHdr + 8) = n + 1;
}
static void AddCosmetic(uint32_t id, uint32_t type, bool listed, bool refused = false, uint32_t unlocks = 0) {
    g_items[g_itemCount++] = { id, type, false, false, refused, unlocks, 0 };
    if (listed) Show(id);
}
static bool GrantsAllowed() { return *(uint8_t*)(g_currentProfile + PROFILE_GRANTS_ON) != 0; }
static uint16_t& Have(uint32_t id) { return *(uint16_t*)(g_account + STORE_OFFSET + (uintptr_t)id * 8 + 6); }
static uint8_t& Seen(uint32_t id) { return *(g_account + STORE_OFFSET + (uintptr_t)id * 8 + 4); }

static uintptr_t __fastcall FakePlayer() { return (uintptr_t)g_entity; }
static uint32_t __fastcall FakeItemType(uint32_t id) { Cosmetic* c = Find(id); return c ? c->type : 0; }
static bool __fastcall FakeOwned(uint32_t id) { Cosmetic* c = Find(id); return c && c->owned; }
static bool __fastcall FakePackOk(uint32_t id) { Cosmetic* c = Find(id); return c && !c->unavailable; }
static bool __fastcall FakeGive(uint32_t id, char) {
    Cosmetic* c = Find(id);
    if (!c) return false;
    c->gives++;
    if (GrantsAllowed() && !c->refused && !c->owned) {
        c->owned = true;
        if (c->unlocks) Show(c->unlocks);
    }
    return g_giveReturns;
}
static uintptr_t __fastcall FakeAccount(uintptr_t) { return (uintptr_t)g_account; }
static bool __fastcall FakePackOwned(uintptr_t, uint32_t) { return true; }
static void __fastcall FakeColGive(uintptr_t, uint8_t* it, uint16_t count, char, char, uint16_t, uint8_t, uint32_t, uint32_t) {
    g_colGives++;
    if (GrantsAllowed()) Have(*(uint32_t*)(it + 0x48)) += count;
}
static uintptr_t __fastcall FakeProfile(uintptr_t) { return g_currentProfile; }
static uintptr_t __fastcall FakeHeroStruct(uintptr_t, uint32_t, char create) {
    if (create) g_heroExists = true;
    return g_heroExists ? (uintptr_t)g_hero : 0;
}
static uint8_t* __fastcall FakeInvSlot(uintptr_t, uint32_t, char) { return g_slotExists ? g_slot : nullptr; }
static void __fastcall FakeInvAdd(uintptr_t, uint32_t) { g_slotExists = true; }
static void __fastcall FakeSound(uintptr_t, const char*, uintptr_t, uintptr_t, uintptr_t, uintptr_t) { g_sounds++; }

static const uint32_t OUTFIT = 0x100, EMOTE = 0x200, TAKEDOWN = 0x300, CHAIN_A = 0x400, CHAIN_B = 0x401;
static const uint32_t COL_HIDDEN = 6, COL_CODEX = 5, COL_COMIC = 7, GIFT_OUTFIT = 0xA11, GIFT_STYLE = 0x5717;

static void NewWorld(FakeImage& im) {
    memset(g_items, 0, sizeof g_items); g_itemCount = 0;
    memset(g_list, 0, sizeof g_list); memset(g_lockHdr, 0, sizeof g_lockHdr);
    *(uint8_t**)g_lockHdr = g_list;
    AddCosmetic(OUTFIT, T_OUTFIT, true);
    AddCosmetic(EMOTE, T_EMOTE, true);
    memset(g_entity, 0, sizeof g_entity);
    *(uintptr_t*)(g_entity + ENTITY_INVENTORY_PART) = (uintptr_t)g_hero;
    memset(g_profile, 0, sizeof g_profile); memset(g_profile2, 0, sizeof g_profile2);
    g_profile[PROFILE_GRANTS_ON] = 1; g_profile2[PROFILE_GRANTS_ON] = 1;
    g_currentProfile = (uintptr_t)g_profile;
    memset(g_account, 0, sizeof g_account);

    memset(g_db, 0, sizeof g_db); memset(g_set, 0, sizeof g_set); memset(g_colItems, 0, sizeof g_colItems);
    *(uint8_t**)(g_db + 0x3C0) = g_set; *(uint32_t*)(g_db + 0x3C8) = 1;
    *(uint8_t**)(g_set + 0x98) = g_colItems; *(uint32_t*)(g_set + 0xA0) = 3;
    uint8_t* hidden = g_colItems;
    *(uint32_t*)(hidden + 0x10) = 0; *(uint16_t*)(hidden + 8) = 1; *(uint32_t*)(hidden + 0x48) = COL_HIDDEN;
    uint8_t* codex = g_colItems + 0x150;
    *(uint32_t*)(codex + 0x10) = K_CODEX; *(uint32_t*)(codex + 0x48) = COL_CODEX;
    uint8_t* comic = g_colItems + 0x2A0;
    *(uint32_t*)(comic + 0x10) = K_COMIC; *(uint32_t*)(comic + 0x48) = COL_COMIC; *(uint32_t*)(comic + 0x140) = 0;
    *(uint8_t**)(g_db + 0x410) = g_tables; *(uint32_t*)(g_db + 0x418) = 1;
    *(uint8_t**)(g_tables + 8) = g_tiers; *(uint32_t*)(g_tables + 0x10) = 2;
    *(uint16_t*)g_tiers = 1; *(uint16_t*)(g_tiers + 0x18) = 3;
    Have(COL_COMIC) = 1;

    memset(g_giftCfg, 0, sizeof g_giftCfg);
    *(uint8_t**)g_cat = g_outfit; *(uint32_t*)(g_cat + 8) = 1;
    *(uint32_t*)(g_outfit + 8) = GIFT_OUTFIT; *(uint32_t*)(g_outfit + 0x10) = 0;
    *(uint8_t**)(g_outfit + 0x130) = g_styles; *(uint32_t*)(g_outfit + 0x138) = 1;
    *(uint32_t*)(g_styles + 8) = GIFT_STYLE;
    *(uint8_t**)(g_heroes + 8) = g_heroArray; *(uint32_t*)(g_heroes + 0x10) = 1;
    *(uint32_t*)(g_heroArray + 8) = 0xE01;
    memset(g_slot, 0, sizeof g_slot);
    g_heroExists = g_slotExists = false;
    g_colGives = g_sounds = 0;
    g_giveReturns = true;
    g_stuck = -1; g_stuckProfile = 0; g_lockedNow = -1;
    memset(&g_tile, 0, sizeof g_tile);
    g_tile = { COLLECTION_TILE_ID, "COLLECTION", "ALL OWNED", "UNLOCK COLLECTION", AllDone, UnlockAll, DoneLabel };
    const struct { uintptr_t rva; const void* fn; } code[] = {
        { RVA_HERO, (const void*)FakePlayer }, { RVA_ITEMTYPE, (const void*)FakeItemType }, { RVA_OWNED, (const void*)FakeOwned },
        { RVA_OUTFITPACKOK, (const void*)FakePackOk }, { RVA_GIVE, (const void*)FakeGive }, { RVA_ACCOUNT, (const void*)FakeAccount },
        { RVA_PACKOWNED, (const void*)FakePackOwned }, { RVA_COLGIVE, (const void*)FakeColGive }, { RVA_PROFILE, (const void*)FakeProfile },
        { RVA_HEROSTRUCT, (const void*)FakeHeroStruct }, { RVA_INVSLOT, (const void*)FakeInvSlot }, { RVA_INVADD, (const void*)FakeInvAdd },
        { RVA_SOUND, (const void*)FakeSound } };
    for (const auto& c : code) CHECK(im.Jump(c.rva, c.fn));
    const struct { uintptr_t rva; const void* value; } data[] = {
        { RVA_LOCKABLE, g_lockHdr }, { RVA_COLDB, g_db }, { RVA_PACKLIST, nullptr }, { RVA_GIFTCFG, g_giftCfg },
        { RVA_OUTFITS, g_cat }, { RVA_HEROES, g_heroes }, { RVA_SOUNDSYS, g_db } };
    for (const auto& d : data) CHECK(im.Put(d.rva, &d.value, 8, PAGE_READWRITE));
    g_base = (uintptr_t)im.base;
    ClearLog();
}

static void OwnAll() {
    Find(OUTFIT)->owned = Find(EMOTE)->owned = true;
    g_heroExists = g_slotExists = true;
    *(uint32_t*)(g_slot + 0x28) = GIFT_STYLE;
    Have(COL_HIDDEN) = 1; Have(COL_CODEX) = 1; Have(COL_COMIC) = 3;
}

static bool GiftGiven() { return g_slotExists && *(uint32_t*)(g_slot + 0x28) == GIFT_STYLE; }
static bool CollectiblesGiven() { return Have(COL_HIDDEN) == 1 && Have(COL_CODEX) == 1 && Have(COL_COMIC) == 3 && (Seen(COL_HIDDEN) & 1); }

static const size_t IMAGE_SIZE = 0x4E10000;

static void TestEverythingGiven() {
    FakeImage im(IMAGE_SIZE);
    NewWorld(im);
    CHECK(!AllDone());
    CHECK_EQ(g_lockedNow, 2 + 1 + 2 + 1);
    UnlockAll();
    CHECK(Find(OUTFIT)->owned && Find(EMOTE)->owned);
    CHECK(GiftGiven());
    CHECK(CollectiblesGiven());
    CHECK(Logged("done: 0 still locked afterwards"));
    CHECK(Logged(" 1 raised to their top tier"));
    CHECK_EQ(g_sounds, 1);
    CHECK(AllDone());
    CHECK(strcmp(DoneLabel(), "ALL OWNED") == 0);
    CHECK_EQ(Find(OUTFIT)->gives, 1);
}

static void TestGrantsOff() {
    FakeImage im(IMAGE_SIZE);
    NewWorld(im);
    g_profile[PROFILE_GRANTS_ON] = 0;
    UnlockAll();
    CHECK(Logged("the game does not take grants for this profile right now - nothing given; press again later"));
    CHECK_EQ(Find(OUTFIT)->gives, 0);
    CHECK_EQ(g_colGives, 0);
    CHECK(!g_slotExists && !g_heroExists);
    CHECK_EQ(Seen(COL_HIDDEN), 0);
    CHECK_EQ(g_stuck, -1);
    CHECK(!AllDone());
    g_profile[PROFILE_GRANTS_ON] = 1;
    UnlockAll();
    CHECK(Find(OUTFIT)->owned && GiftGiven() && CollectiblesGiven());
    CHECK(AllDone());
}

static void TestRefused() {
    {
        FakeImage im(IMAGE_SIZE);
        NewWorld(im);
        AddCosmetic(TAKEDOWN, T_TAKEDOWN, true, true);
        UnlockAll();
        CHECK(Find(OUTFIT)->owned && !Find(TAKEDOWN)->owned);
        CHECK_EQ(Find(TAKEDOWN)->gives, 2);
        CHECK(Logged("done: 1 still locked afterwards"));
        CHECK(AllDone());
        CHECK(strcmp(DoneLabel(), "ALL GIVEN") == 0);
        CHECK(strcmp(TileTitle(g_tile, true), "ALL GIVEN") == 0);

        g_currentProfile = (uintptr_t)g_profile2;
        CHECK(!AllDone());
        g_currentProfile = (uintptr_t)g_profile;
    }
    {
        FakeImage im(IMAGE_SIZE);
        NewWorld(im);
        AddCosmetic(TAKEDOWN, T_TAKEDOWN, true, true);
        g_giveReturns = false;
        UnlockAll();
        CHECK(Find(OUTFIT)->owned);
        CHECK_EQ(Find(TAKEDOWN)->gives, 2);
    }
}

static void TestGiveUnlocksMore() {
    FakeImage im(IMAGE_SIZE);
    NewWorld(im);
    OwnAll();
    AddCosmetic(TAKEDOWN, T_TAKEDOWN, true, true);
    AddCosmetic(CHAIN_A, T_OUTFIT, true, false, CHAIN_B);
    AddCosmetic(CHAIN_B, T_OUTFIT, false);
    UnlockAll();
    CHECK(Find(CHAIN_A)->owned);
    CHECK(Find(CHAIN_B)->owned);
    CHECK_EQ(Find(CHAIN_B)->gives, 1);
    CHECK_EQ(Find(TAKEDOWN)->gives, 3);
    CHECK(Logged("done: 1 still locked afterwards"));
}

static uint8_t g_column[0x300];
static int g_columnIds[4];
static uint8_t g_nav[0x100];
static void* g_columnVtbl[80];
static void __fastcall FakeColumnRefresh(uint8_t*) {}

static void Column() {
    memset(g_column, 0, sizeof g_column);
    g_columnVtbl[SLOT_REFRESH] = (void*)FakeColumnRefresh;
    *(void**)g_column = g_columnVtbl;
    g_columnIds[0] = 0xC7; g_columnIds[1] = COLLECTION_TILE_ID;
    *(uint32_t*)(g_column + 0x238) = 2;
    *(int**)(g_column + 0x240) = g_columnIds;
    *(uint8_t**)(g_column + 0x188) = g_nav;
    *(int*)(g_nav + 0x84) = 1;
}

static void TestTile() {
    FakeImage im(IMAGE_SIZE);
    NewWorld(im);
    Column();
    g_profile[PROFILE_GRANTS_ON] = 0;
    CHECK(TileAction(g_tile, g_column, ACCEPT));
    Sleep(DOUBLE_CLICK_MS + 50);
    CHECK(TileAction(g_tile, g_column, ACCEPT));
    CHECK(Logged("nothing given; press again later"));
    CHECK(!TileDone(g_tile));
    CHECK(strcmp(TileTitle(g_tile, false), "COLLECTION") == 0);
    g_profile[PROFILE_GRANTS_ON] = 1;
    Sleep(DOUBLE_CLICK_MS + 50);
    CHECK(TileAction(g_tile, g_column, ACCEPT));
    Sleep(DOUBLE_CLICK_MS + 50);
    CHECK(TileAction(g_tile, g_column, ACCEPT));
    CHECK(Find(OUTFIT)->owned && GiftGiven());
    g_tile.doneAt = 0;
    CHECK(TileDone(g_tile));
    CHECK(strcmp(TileTitle(g_tile, true), "ALL OWNED") == 0);
    CHECK(!g_tile.faulted);
}

int main() {
    wchar_t base[MAX_PATH];
    GetTempPathW(MAX_PATH, base);
    _snwprintf_s(g_tmp, _countof(g_tmp), _TRUNCATE, L"%sCollectionUnlockTests-%lu\\", base, GetCurrentProcessId());
    CreateDirectoryW(g_tmp, nullptr);
    wcscpy_s(g_dir, g_tmp);
    TestEverythingGiven();
    TestGrantsOff();
    TestRefused();
    TestGiveUnlocksMore();
    TestTile();
    ClearLog();
    RemoveDirectoryW(g_tmp);
    return TestSummary("CollectionUnlockTests");
}
