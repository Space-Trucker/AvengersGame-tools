# Changelog

All notable changes per tool. Dates are YYYY-MM-DD.

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
