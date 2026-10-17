# Changelog

All notable changes per tool. Dates are YYYY-MM-DD.

## 2026-10-17: Collection Unlock 1.0, Quick Start 1.0, Skill Unlock 1.1

### Collection Unlock 1.0 (new)
- **COLLECTION** button on the Character screen, under Skill Unlock's tile. It needs a second press to
  confirm, then gives everything still locked, for every hero, with no popups:
  outfits (with every style option), emotes, takedowns, nameplates, and all collectibles (comics at
  their top tier, intel, codex entries), through the game's own reward and pickup code, so linked rewards fire too.
- Also unlocks the final update's Founder's Gift, Iron Man's Variable Threat Response suit, which an
  offline copy can't earn (it needs a Steam achievement from before April 2023). Stays unlocked for good.
- Leaves content that isn't part of the PC game alone (Spider-Man, styles marked unreleased).
- Changes no game code: it only uses the game's own functions from the tile.
- Shows ALL OWNED when done. Works alone or together with Skill Unlock, in either load order.

### Quick Start 1.0 (new)
- Keeps the game loading and running when its window is not in front.
- Skips the photosensitivity warning, the legal screen's wait, the intro movie and "press any key"; the
  profile and save load normally and the main menu opens.
- Each part can be switched off in `QuickStart.ini`; any part whose game code differs is skipped and logged.

### Skill Unlock 1.1
- Works together with Collection Unlock in the same column, in either load order. If only one tile slot
  is free, UNLOCK ALL gets it.
- Pressing a tile redraws the whole column, so the other mod's tile keeps its text.
- Refuses to install twice (two copies in `mods`) instead of replacing the first copy.
- Logs "tile added" once per session instead of every time the screen opens.

## 2026-10-17: first public release

### Avengers Mod Loader 1.1
- Loads `mods\*.asi` and passes them the ScriptHook 1.0 mod interface (`MatchRva`, `PatchRva`,
  `FindPattern`), also exported as `ASH_*`.
- Passes Windows' own `version.dll` functions on to the system's `version.dll`, found at run time.
- Loads only files ending exactly in `.asi` (so `mymod.asi_off` stays disabled).
- Logs to `mods\ModLoader.log`.

### Skill Unlock 1.0
- **UNLOCK ALL** button on the Character screen. It needs a second press to confirm, levels the current
  hero to 50 and max champion level, buys every skill on every tab and shows ALL MAXED when done.
- Works with Avengers ScriptHook and with Avengers Mod Loader.
- After an unexpected error it switches itself off until the game restarts.

### Avengers Save Tool 1.0
- Unlock the war table and all heroes on a save; import a save shared by another player; back up and
  restore.
- Only works with the game closed, backs up before every change, writes atomically, and rejects damaged
  or oversized files.
