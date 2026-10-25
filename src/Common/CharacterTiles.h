#pragma once
#include "ModCommon.h"
#include <wchar.h>

namespace tiles {
static const uintptr_t RVA_VTBL = 0x2C309D8;
static const uintptr_t RVA_LAYOUT = 0x22B3780, RVA_REFRESH = 0x2344520, RVA_ACTION = 0x22E6030;
static const uintptr_t RVA_SETSTATE = 0x2378D00, RVA_LOCALIZE = 0x172FC30, RVA_LOCTABLE = 0x45D9DF0;
static const uintptr_t RVA_PUSH = 0x22702E0;
static const uintptr_t RVA_HERO = 0x1D8F940;
static const uintptr_t RVA_SOUND = 0x1B1B9A0, RVA_SOUNDSYS = 0x4DDC3B8;
static const uintptr_t ENTITY_INVENTORY_PART = 0x3490;
static const int SLOT_ACTION = 26, SLOT_LAYOUT = 71, SLOT_REFRESH = 75;
static const int COLUMN_SLOTS = 4, ACCEPT = 0x1A1;
static const int SKILL_TILE_ID = 0x7F00, COLLECTION_TILE_ID = 0x7F01;
static const wchar_t* const TILE_MUTEX = L"Local\\AvengersCharacterTiles";
static const DWORD TILE_LOCK_WAIT_MS = 10000;

typedef void (__fastcall *Fn1)(uint8_t*);
typedef void (__fastcall *ActionFn)(uint8_t*, int, uint8_t);
typedef void (__fastcall *SetStateFn)(uint8_t*, uint32_t, uint8_t, uint8_t, uint8_t, const char*, int, const char*);
typedef const char* (__fastcall *LocFn)(void*, const char*);
typedef void (__fastcall *PushFn)(void*, const int*);
typedef uintptr_t (__fastcall *PtrFn0)();
typedef void (__fastcall *SoundFn)(uintptr_t, const char*, uintptr_t, uintptr_t, uintptr_t, uintptr_t);
typedef int (__cdecl *TileChainFn)();

static inline uintptr_t PlayerEntity() { return ((PtrFn0)(g_base + RVA_HERO))(); }
static inline uintptr_t InventoryPart(uintptr_t entity) { return entity ? *(uintptr_t*)(entity + ENTITY_INVENTORY_PART) : 0; }

static inline void PlayUnlockSound(uintptr_t entity) {
    uintptr_t part = InventoryPart(entity);
    if (part) ((SoundFn)(g_base + RVA_SOUND))(*(uintptr_t*)(g_base + RVA_SOUNDSYS), "UI_Skills_Pick3SkillPurchased", part, 0, 0, 0);
}

static inline uint32_t Count(uint8_t* w) { return *(uint32_t*)(w + 0x238); }
static inline int* Ids(uint8_t* w) { return *(int**)(w + 0x240); }
static inline int IndexOf(uint8_t* w, int id) {
    for (uint32_t i = 0; i < Count(w) && i < 16; i++) if (Ids(w)[i] == id) return (int)i;
    return -1;
}
static inline int Selected(uint8_t* w) {
    uint8_t* nav = *(uint8_t**)(w + 0x188);
    return nav ? *(int*)(nav + 0x84) : -1;
}
static inline bool HasMovie(uint8_t* w) { return *(void**)(w + 0xB8) != nullptr; }
static inline void PushTile(uint8_t* w, const int* id) { ((PushFn)(g_base + RVA_PUSH))(w + 0x238, id); }
static inline const char* SelectHint() {
    return ((LocFn)(g_base + RVA_LOCALIZE))(*(void**)(g_base + RVA_LOCTABLE), "BL_Select");
}
static inline void SetTile(uint8_t* w, int idx, const char* title, bool clickable) {
    ((SetStateFn)(g_base + RVA_SETSTATE))(w, (uint32_t)idx, 0, Selected(w) == idx, 0, title, clickable ? ACCEPT : 0, SelectHint());
}

static inline void RefreshColumn(uint8_t* w) { ((Fn1)(*(uintptr_t**)w)[SLOT_REFRESH])(w); }

static const DWORD CONFIRM_MS = 4000;
static const DWORD DOUBLE_CLICK_MS = 400;
static const DWORD DONE_CACHE_MS = 500;

static inline bool IsOwnAccept(uint8_t* w, int id, int action) {
    if (action != ACCEPT) return false;
    int i = IndexOf(w, id);
    return i >= 0 && Selected(w) == i;
}

static inline bool SamePress(ULONGLONG& last) {
    ULONGLONG now = GetTickCount64();
    if (now - last < DOUBLE_CLICK_MS) return true;
    last = now;
    return false;
}

struct TileButton {
    int id;
    const char *idleText, *doneText, *what;
    bool (*allDone)();
    void (*run)();
    const char* (*doneLabel)();
    bool faulted;
    DWORD armedAt, doneAt;
    ULONGLONG lastPress;
    bool done;
};

static inline bool TileDone(TileButton& b) {
    DWORD now = GetTickCount();
    if (!b.doneAt || now - b.doneAt > DONE_CACHE_MS) { b.done = b.allDone(); b.doneAt = now ? now : 1; }
    return b.done;
}

static inline const char* TileTitle(TileButton& b, bool done) {
    if (done) return b.doneLabel ? b.doneLabel() : b.doneText;
    if (b.armedAt && GetTickCount() - b.armedAt < CONFIRM_MS) return "CONFIRM?";
    return b.idleText;
}

static inline void TileRefresh(TileButton& b, uint8_t* w) {
    if (b.faulted) return;
    __try {
        int i = IndexOf(w, b.id);
        if (i < 0 || !HasMovie(w)) return;
        bool done = TileDone(b);
        SetTile(w, i, TileTitle(b, done), !done);
    } FAULT { b.faulted = true; Log("fault drawing the tile - %s is off until the game restarts", b.what); }
}

static inline bool TileAction(TileButton& b, uint8_t* w, int action) {
    bool mine = false;
    __try { mine = IsOwnAccept(w, b.id, action); } FAULT {}
    if (!mine) return false;
    if (b.faulted) return true;
    if (SamePress(b.lastPress)) return true;
    DWORD now = GetTickCount();
    bool done = false;
    __try { done = b.allDone(); } FAULT { b.faulted = true; Log("fault checking - %s is off until the game restarts", b.what); return true; }
    if (done) { b.armedAt = 0; return true; }
    if (!b.armedAt || now - b.armedAt >= CONFIRM_MS) {
        b.armedAt = now ? now : 1;
        Log("%s: waiting for confirmation", b.what);
    } else {
        b.armedAt = 0;
        Log("%s confirmed", b.what);

        __try { b.run(); } FAULT {
            b.faulted = true;
            Log("fault during %s - it stopped part-way and is off until the game restarts; restart the game before playing on",
                b.what);
            return true;
        }
        b.doneAt = 0;
    }
    __try { RefreshColumn(w); } FAULT {}
    return true;
}

enum SlotOwnerKind { OWNER_GAME, OWNER_SKILL, OWNER_COLLECTION, OWNER_CHAIN, OWNER_UNKNOWN };

struct KnownTileMod { const wchar_t* prefix; const wchar_t* file; SlotOwnerKind owner; };
static const KnownTileMod TILE_MODS[] = {
    { L"SkillUnlock", L"SkillUnlock.asi", OWNER_SKILL },
    { L"CollectionUnlock", L"CollectionUnlock.asi", OWNER_COLLECTION },
};

static const char* const TILE_CHAIN_EXPORT = "AvengersTileChain";
static const int TILE_CHAIN_VERSION = 1;

static inline bool StartsWithI(const wchar_t* s, const wchar_t* prefix) { return _wcsnicmp(s, prefix, wcslen(prefix)) == 0; }

static inline SlotOwnerKind SlotOwner(uintptr_t p, uintptr_t gameRva, HMODULE* module = nullptr) {
    if (module) *module = nullptr;
    if (p == g_base + gameRva) return OWNER_GAME;
    HMODULE m = nullptr;
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                            (LPCWSTR)p, &m)) return OWNER_UNKNOWN;
    if (module) *module = m;
    wchar_t path[MAX_PATH] = L"";
    DWORD n = GetModuleFileNameW(m, path, MAX_PATH);
    if (n && n < MAX_PATH) {
        const wchar_t* name = wcsrchr(path, L'\\'); name = name ? name + 1 : path;
        for (const KnownTileMod& k : TILE_MODS) if (StartsWithI(name, k.prefix)) return k.owner;
    }
    TileChainFn chain = (TileChainFn)GetProcAddress(m, TILE_CHAIN_EXPORT);
    return chain && chain() >= TILE_CHAIN_VERSION ? OWNER_CHAIN : OWNER_UNKNOWN;
}

struct TileHooks { Fn1 layout, refresh; ActionFn action; };

static inline const char* InstallTile(SlotOwnerKind self, Fn1 layout, Fn1 refresh, ActionFn action,
                                      TileHooks& next, SlotOwnerKind& under) {
    uintptr_t* v = (uintptr_t*)(g_base + RVA_VTBL);
    const int slots[3] = { SLOT_LAYOUT, SLOT_REFRESH, SLOT_ACTION };
    const uintptr_t gameRvas[3] = { RVA_LAYOUT, RVA_REFRESH, RVA_ACTION };
    const uintptr_t ours[3] = { (uintptr_t)layout, (uintptr_t)refresh, (uintptr_t)action };
    uintptr_t held[3];
    SlotOwnerKind owner[3];
    HMODULE module[3];
    for (int i = 0; i < 3; i++) { held[i] = v[slots[i]]; owner[i] = SlotOwner(held[i], gameRvas[i], &module[i]); }
    for (int i = 0; i < 3; i++) if (owner[i] == self) return "this mod is already installed (two copies in mods?)";
    for (int i = 0; i < 3; i++) if (owner[i] == OWNER_UNKNOWN) return "character screen code differs (another mod?)";
    for (int i = 1; i < 3; i++)
        if (owner[i] != owner[0] || module[i] != module[0]) return "character screen hooks are half installed (another mod?)";
    next.layout = (Fn1)held[0];
    next.refresh = (Fn1)held[1];
    next.action = (ActionFn)held[2];
    under = owner[0];
    for (int i = 0; i < 3; i++) {
        if (SwapPointer(v + slots[i], held[i], ours[i])) continue;
        while (i--) SwapPointer(v + slots[i], ours[i], held[i]);
        return "could not change the character screen safely";
    }
    return nullptr;
}

struct TileLock {
    HANDLE h; bool owned;
    TileLock() : h(CreateMutexW(nullptr, FALSE, TILE_MUTEX)), owned(false) {
        if (h) { DWORD r = WaitForSingleObject(h, TILE_LOCK_WAIT_MS); owned = r == WAIT_OBJECT_0 || r == WAIT_ABANDONED; }
    }
    ~TileLock() { if (owned) ReleaseMutex(h); if (h) CloseHandle(h); }
    TileLock(const TileLock&) = delete;
    TileLock& operator=(const TileLock&) = delete;
    bool ok() const { return owned; }
};
}
