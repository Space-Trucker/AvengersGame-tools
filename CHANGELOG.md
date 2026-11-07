# Changelog

All notable changes per tool. Dates are YYYY-MM-DD.

## 2026-11-07: Avengers Mod Loader no longer maintained

It won't get updates or support. Please use Avengers ScriptHook instead. The source and version 1.2 stay available here.

## 2026-10-25: fixes for every tool

### Avengers Mod Loader 1.2
- `PatchRva` is safe when mods call it from their own threads at the same time: each call holds a process-wide
  lock and keeps its own record of the pages' rights. In 1.1 two calls at once shared one record and could leave
  a page of game code with the wrong rights.
- Log lines written by different threads no longer mix.
- Keeps the thread notifications that the C runtime it is built with needs (1.1 turned them off).

### Skill Unlock 1.2
- If an unexpected error stops the unlock part way, the skill points and champion points are still put back
  (1.1 skipped that step), and the log says to restart the game before playing on.
- After such an error the tile keeps the presses on itself; 1.1 passed them on to the game, which does not know
  the tile.
- Ranked skills are bought only while the hero's trait manager can be read, which the game's own buy button
  uses to drop the lower rank; otherwise they are skipped and the log says so.
- Skill nodes of a type the game does not have are skipped and logged, instead of being bought as three-choice
  nodes.
- Installing into the Character screen: each hook is swapped only if it still holds what was read, under the
  same process-wide lock as Avengers Mod Loader 1.2's `PatchRva`, and a failure puts back the ones already
  swapped. Other tile mods that export `AvengersTileChain` (see `src/Common/CharacterTiles.h`) can share the
  column in any load order.
- A game folder path too long for the log file no longer ends the game; the log line is skipped.

### Collection Unlock 1.1
- When the game does not take grants for the current profile yet, nothing is given, the log says so, and the
  tile can be pressed again later. 1.0 then showed ALL OWNED with everything still locked.
- The tile reads ALL OWNED only when nothing is locked. When the game keeps something locked, it reads
  ALL GIVEN and the log lists what is left.
- The repeat passes go on while what is locked changes, and stop when a pass changes nothing; 1.0 stopped on
  what the game's give function returned.
- The same Character screen fixes as Skill Unlock 1.2 (presses after an error, safe install, long paths).
- Correction to the 1.0 notes: besides using the game's own functions, it writes two values directly: the
  "seen" flag of collectibles the game hides until they are found, and the Founder's Gift outfit's style.

### Quick Start 1.1
- The background patch is made the hot-patch way: `return 0` is first written into the unused padding before
  the game's function, then only the function's first instruction (2 bytes) becomes a jump to it, in one
  atomic write. A game thread inside the function at that moment is safe; 1.0 replaced its first six bytes.
  Quick Start 1.0's patch is recognised.
- The three photosensitivity-warning patches are applied all together or not at all.
- `QuickStart.ini` is no longer in the archive. Quick Start writes it, with every part on, at its first start
  when there is none, so an update never resets your settings. Without the file every part is on.
- Every change it makes to game code holds the same process-wide lock as Avengers Mod Loader 1.2's
  `PatchRva`, and the press-start hook is swapped only if it still holds the game's function.
- A game folder path too long for the log file no longer ends the game; the log line is skipped.

### Avengers Save Tool 1.1
- Import and Restore replace the saves as one change: the new files are written in full and read back first,
  and only then are the old saves swapped out. If anything fails, the save folder is left as it was. 1.0
  deleted the old saves before writing the new ones.
- Restore refuses a backup that holds a file too large to be a save, names it, and changes nothing.
- Imported saves are checked by decoding them back, as Unlock already did.
- The game is checked to be closed again right before writing (it can be started while a dialog is open).
- Each file is flushed to the disk before it replaces the old one.
- A folder named with more digits than a Steam ID can hold no longer causes an error.
- The story stage is searched for in every block of its kind (every save seen has it in the first).
- Needs .NET Framework 4.5 or later, part of Windows 10 and 11 (1.0's notes said 4.x).

### Repository
- Every tool's version is in `versions.txt`; the build checks it against the docs and this file.
- `scripts\test.bat` builds and runs the tests in `tests\`; CI runs them on every push.
- Builds are reproducible (`/Brepro`) and make `.pdb` files, which are kept out of the archives.

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
