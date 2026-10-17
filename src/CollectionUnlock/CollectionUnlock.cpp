#define MOD_NAME L"CollectionUnlock"
#include "..\Common\CharacterTiles.h"

static const uintptr_t RVA_LOCKABLE = 0x4D96DD0, RVA_ITEMTYPE = 0x20F4090, RVA_OWNED = 0x2105F10;
static const uintptr_t RVA_GIVE = 0x21138A0, RVA_OUTFITPACKOK = 0x1F89450;
static const uintptr_t RVA_COLDB = 0x4E04DB8, RVA_ACCOUNT = 0x1F7C9D0, RVA_COLGIVE = 0x1ACDC10;
static const uintptr_t RVA_PACKLIST = 0x4DDC858, RVA_PACKOWNED = 0x1B38140;
static const uintptr_t RVA_GIFTCFG = 0x4D96E80, RVA_OUTFITS = 0x4D96E48, RVA_HEROES = 0x4D96E18;
static const uintptr_t RVA_PROFILE = 0x1F79DF0, RVA_HEROSTRUCT = 0x1D8E3A0;
static const uintptr_t RVA_INVSLOT = 0x1F7BB10, RVA_INVADD = 0x1F83970;
static const uint32_t STORE_OFFSET = 0x500C, STORE_SIZE = 0x400;

typedef uint32_t (__fastcall *ItemTypeFn)(uint32_t);
typedef bool (__fastcall *OwnedFn)(uint32_t);
typedef bool (__fastcall *GiveFn)(uint32_t, char);
typedef bool (__fastcall *OutfitPackOkFn)(uint32_t);
typedef uintptr_t (__fastcall *AccountFn)(uintptr_t);
typedef bool (__fastcall *PackOwnedFn)(uintptr_t, uint32_t);
typedef void (__fastcall *CollectibleGiveFn)(uintptr_t, uint8_t*, uint16_t, char, char, uint16_t, uint8_t, uint32_t, uint32_t);
typedef uintptr_t (__fastcall *ProfileFn)(uintptr_t);
typedef uintptr_t (__fastcall *HeroStructFn)(uintptr_t, uint32_t, char);
typedef uint8_t* (__fastcall *InvSlotFn)(uintptr_t, uint32_t, char);
typedef void (__fastcall *InvAddFn)(uintptr_t, uint32_t);

enum { T_OUTFIT = 2, T_EMOTE, T_TAKEDOWN, T_NAMEPLATE, T_COUNT };
enum { K_INTEL = 1, K_COMIC = 0x10, K_CODEX = 0x11, K_COUNT = 0x20 };

struct Tally {
    int locked[T_COUNT], given[T_COUNT], unavailable, gift;
    int col, colGiven, colKinds[K_COUNT], colUnavailable;
    int comicTiers, comicsMaxed;
};

static const char* TypeName(int t) {
    switch (t) { case T_OUTFIT: return "outfits"; case T_EMOTE: return "emotes"; case T_TAKEDOWN: return "takedowns"; case T_NAMEPLATE: return "nameplates"; }
    return "other";
}

static bool WalkCosmetics(bool grant, Tally& t) {
    uint8_t* hdr = *(uint8_t**)(g_base + RVA_LOCKABLE);
    if (!hdr) return false;
    uint8_t* list = *(uint8_t**)hdr;
    uint32_t n = *(uint32_t*)(hdr + 8);
    if (!list || !n || n > 100000) return false;
    for (uint32_t i = 0; i < n; i++) {
        uint32_t id = *(uint32_t*)(list + (uintptr_t)i * 0x18 + 4);
        uint32_t ty = ((ItemTypeFn)(g_base + RVA_ITEMTYPE))(id);
        if (ty < T_OUTFIT || ty >= T_COUNT) continue;
        if (((OwnedFn)(g_base + RVA_OWNED))(id)) continue;
        if (ty == T_OUTFIT && !((OutfitPackOkFn)(g_base + RVA_OUTFITPACKOK))(id)) { t.unavailable++; continue; }
        t.locked[ty]++;
        if (grant && ((GiveFn)(g_base + RVA_GIVE))(id, 1)) t.given[ty]++;
    }
    return true;
}

static uint16_t TargetCopies(uint8_t* db, uint8_t* item) {
    uint16_t target = 1;
    if (*(uint32_t*)(item + 0x10) != K_COMIC) return target;
    uint8_t* tables = *(uint8_t**)(db + 0x410);
    uint32_t idx = *(uint32_t*)(item + 0x140);
    if (!tables || idx >= *(uint32_t*)(db + 0x418)) return target;
    uint8_t* tiers = *(uint8_t**)(tables + (uintptr_t)idx * 0x18 + 8);
    uint32_t n = *(uint32_t*)(tables + (uintptr_t)idx * 0x18 + 0x10);
    for (uint32_t j = 0; tiers && j < n && j < 32; j++) {
        uint16_t need = *(uint16_t*)(tiers + (uintptr_t)j * 0x18);
        if (need > target) target = need;
    }
    return target;
}

static bool WalkCollectibles(bool grant, Tally& t) {
    uintptr_t entity = PlayerEntity();
    uint8_t* db = *(uint8_t**)(g_base + RVA_COLDB);
    if (!entity || !db) return false;
    uintptr_t account = ((AccountFn)(g_base + RVA_ACCOUNT))(entity);
    if (!account) return false;
    uint8_t* store = (uint8_t*)account + STORE_OFFSET;
    uint8_t* sets = *(uint8_t**)(db + 0x3C0);
    uint32_t ns = *(uint32_t*)(db + 0x3C8);
    if (!sets || ns > 10000) return false;
    uintptr_t packList = *(uintptr_t*)(g_base + RVA_PACKLIST);
    for (int pass = grant ? 0 : 1; pass < 2; pass++)
        for (uint32_t si = 0; si < ns; si++) {
            uint8_t* set = sets + (uintptr_t)si * 0xA8;
            uint8_t* items = *(uint8_t**)(set + 0x98);
            uint32_t ni = *(uint32_t*)(set + 0xA0);
            if (!items || ni > 10000) continue;

            if (set[0x92] && !(packList && ((PackOwnedFn)(g_base + RVA_PACKOWNED))(packList, *(uint32_t*)(set + 0x94)))) {
                if (pass == 1) t.colUnavailable += (int)ni;
                continue;
            }
            for (uint32_t k = 0; k < ni; k++) {
                uint8_t* it = items + (uintptr_t)k * 0x150;
                uint32_t id = *(uint32_t*)(it + 0x48), kind = *(uint32_t*)(it + 0x10);
                if (id >= STORE_SIZE) continue;
                uint8_t* rec = store + (uintptr_t)id * 8;
                if (pass == 0) {
                    bool hidden = (kind == 0 && *(uint16_t*)(it + 8)) || (kind == K_INTEL && it[0x54]);
                    if (hidden) rec[4] |= 1;
                    continue;
                }
                uint16_t have = *(uint16_t*)(rec + 6), target = TargetCopies(db, it);
                if (have >= target) continue;
                if (have) t.comicTiers++; else { t.col++; if (kind < K_COUNT) t.colKinds[kind]++; }
                if (grant) {
                    ((CollectibleGiveFn)(g_base + RVA_COLGIVE))(entity, it, (uint16_t)(target - have), 0, 0, 0xFFFF, 0, 7, 0);
                    uint16_t now = *(uint16_t*)(rec + 6);
                    if (!have && now) t.colGiven++;
                    if (now >= target && kind == K_COMIC) t.comicsMaxed++;
                }
            }
        }
    return true;
}

struct GiftOutfit { uint32_t outfit, style, hero; };

static bool FindGift(GiftOutfit& g) {
    uint8_t* cfg = *(uint8_t**)(g_base + RVA_GIFTCFG);
    uint8_t* cat = *(uint8_t**)(g_base + RVA_OUTFITS);
    uint8_t* heroes = *(uint8_t**)(g_base + RVA_HEROES);
    if (!cfg || !cat || !*(uint8_t**)cat || !heroes || !*(uint8_t**)(heroes + 8)) return false;
    uint32_t idx = *(uint32_t*)(cfg + 0x44);
    if (idx >= *(uint32_t*)(cat + 8)) return false;
    uint8_t* of = *(uint8_t**)cat + (uintptr_t)idx * 0x160;
    uint8_t* styles = *(uint8_t**)(of + 0x130);
    uint32_t heroIdx = *(uint32_t*)(of + 0x10);
    if (!styles || !*(uint32_t*)(of + 0x138) || heroIdx >= *(uint32_t*)(heroes + 0x10)) return false;
    g.outfit = *(uint32_t*)(of + 8);
    g.style = *(uint32_t*)(styles + 8);
    g.hero = *(uint32_t*)(*(uint8_t**)(heroes + 8) + (uintptr_t)heroIdx * 0x248 + 8);
    return g.outfit != 0 && g.style != 0;
}

static uint8_t* GiftSlot(const GiftOutfit& g, uintptr_t profile, bool create) {
    uintptr_t hero = ((HeroStructFn)(g_base + RVA_HEROSTRUCT))(profile, g.hero, create ? 1 : 0);
    if (!hero) return nullptr;
    uint8_t* slot = ((InvSlotFn)(g_base + RVA_INVSLOT))(hero, g.outfit, 0);
    if (!slot && create) {
        ((InvAddFn)(g_base + RVA_INVADD))(hero, g.outfit);
        slot = ((InvSlotFn)(g_base + RVA_INVSLOT))(hero, g.outfit, 0);
    }
    return slot;
}

static uintptr_t CurrentProfile() {
    uintptr_t entity = PlayerEntity();
    return entity ? ((ProfileFn)(g_base + RVA_PROFILE))(entity) : 0;
}

static void WalkGift(bool grant, Tally& t) {
    uintptr_t profile = CurrentProfile();
    GiftOutfit g;
    if (!profile || !FindGift(g)) return;
    uint8_t* slot = GiftSlot(g, profile, false);
    if (slot && *(uint32_t*)(slot + 0x28)) return;
    t.locked[T_OUTFIT]++; t.gift++;
    if (!grant || !*(uint8_t*)(profile + 0x630)) return;
    slot = GiftSlot(g, profile, true);
    if (!slot) return;
    *(uint32_t*)(slot + 0x28) = g.style;
    t.given[T_OUTFIT]++;
    Log("  Founder's Gift outfit %08x (style %08x) unlocked for hero %08x", g.outfit, g.style, g.hero);
}

static bool Walk(bool grant, Tally& t) {
    memset(&t, 0, sizeof t);
    bool cosmetics = WalkCosmetics(grant, t);
    WalkGift(grant, t);
    bool collectibles = WalkCollectibles(grant, t);
    return cosmetics && collectibles;
}

static int Locked(const Tally& t) {
    return t.locked[T_OUTFIT] + t.locked[T_EMOTE] + t.locked[T_TAKEDOWN] + t.locked[T_NAMEPLATE] + t.col + t.comicTiers;
}

static int g_stuck = -1;
static uintptr_t g_stuckProfile;

static void UnlockAll() {
    uintptr_t entity = PlayerEntity();
    if (!entity) { Log("no hero"); return; }
    Tally total;
    bool readable = true;
    for (int pass = 0; pass < 8; pass++) {
        Tally t;
        readable = Walk(true, t);
        int given = t.colGiven + t.comicsMaxed;
        for (int ty = T_OUTFIT; ty < T_COUNT; ty++) given += t.given[ty];
        if (pass == 0) {
            total = t;
        } else {
            for (int ty = T_OUTFIT; ty < T_COUNT; ty++) total.given[ty] += t.given[ty];
            total.colGiven += t.colGiven;
            total.comicsMaxed += t.comicsMaxed;
        }
        if (!given) break;
    }
    if (!readable) Log("warning: the cosmetics or the collectibles list is not loaded; gave what was there");
    for (int ty = T_OUTFIT; ty < T_COUNT; ty++)
        Log("  %-12s %4d locked, %4d given", TypeName(ty), total.locked[ty], total.given[ty]);
    if (total.gift) Log("  (outfits include the Founder's Gift)");
    Log("  %-12s %4d missing, %4d given (intel %d, comics %d, codex %d, other %d)", "collectibles", total.col,
        total.colGiven, total.colKinds[K_INTEL], total.colKinds[K_COMIC], total.colKinds[K_CODEX],
        total.col - total.colKinds[K_INTEL] - total.colKinds[K_COMIC] - total.colKinds[K_CODEX]);
    Log("  %-12s %4d raised to their top tier", "comics", total.comicsMaxed);
    Tally after;
    readable = Walk(false, after);
    Log("done: %d still locked afterwards (outfits %d, emotes %d, takedowns %d, nameplates %d, collectibles %d, "
        "comics below top tier %d); "
        "not available in this version: %d outfit(s), %d collectible(s)", Locked(after), after.locked[T_OUTFIT],
        after.locked[T_EMOTE], after.locked[T_TAKEDOWN], after.locked[T_NAMEPLATE], after.col, after.comicTiers, after.unavailable,
        after.colUnavailable);
    g_stuck = readable ? Locked(after) : -1;
    g_stuckProfile = CurrentProfile();
    PlayUnlockSound(entity);
}

static bool AllDone() {
    Tally t;
    if (!Walk(false, t)) return false;
    int n = Locked(t);
    return n == 0 || (n == g_stuck && CurrentProfile() == g_stuckProfile);
}

static TileHooks g_next;
static SlotOwnerKind g_under;
static TileButton g_tile = { COLLECTION_TILE_ID, "COLLECTION", "ALL OWNED", "UNLOCK COLLECTION", AllDone, UnlockAll };

static void __fastcall Layout(uint8_t* w) {
    if (!g_tile.faulted) __try {
        uint32_t room = (uint32_t)COLUMN_SLOTS - (g_under == OWNER_SKILL && IndexOf(w, SKILL_TILE_ID) < 0 ? 1 : 0);
        if (IndexOf(w, g_tile.id) < 0 && Count(w) < room) {
            PushTile(w, &g_tile.id);
            static bool logged;
            if (!logged) { logged = true; Log("tile added"); }
        }
    } FAULT { g_tile.faulted = true; Log("fault adding the tile - Collection Unlock is off until the game restarts"); }
    g_next.layout(w);
    if (!g_tile.faulted) __try {
        int s = IndexOf(w, SKILL_TILE_ID), m = IndexOf(w, g_tile.id);
        if (s >= 0 && m >= 0 && m < s) { Ids(w)[m] = SKILL_TILE_ID; Ids(w)[s] = g_tile.id; }
    } FAULT { g_tile.faulted = true; Log("fault ordering the tiles - Collection Unlock is off until the game restarts"); }
}

static void __fastcall Refresh(uint8_t* w) { g_next.refresh(w); TileRefresh(g_tile, w); }
static void __fastcall Action(uint8_t* w, int action, uint8_t flag) {
    if (!TileAction(g_tile, w, action)) g_next.action(w, action, flag);
}

static void ModMain() {
    TileLock lock;
    if (!lock.ok()) { Log("could not take the install lock - not installed"); return; }
    if (const char* err = InstallTile(OWNER_COLLECTION, Layout, Refresh, Action, g_next, g_under)) {
        Log("%s - not installed", err); return;
    }
    Log("Collection Unlock 1.0 ready: Character screen > COLLECTION (press twice to confirm)%s",
        g_under == OWNER_SKILL ? ", together with Skill Unlock" : "");
}
