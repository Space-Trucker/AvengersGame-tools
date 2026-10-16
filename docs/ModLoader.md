# Avengers Mod Loader 1.1

by Space Trucker · Marvel's Avengers (PC, Steam version 2.8.2)

A small loader for native mods. At game start it loads every `*.asi` file in the `mods` folder next to
the game.

It is an **alternative** to Avengers ScriptHook, not an add-on: both are a file called `version.dll`, so
install only one of them. It passes the same interface to mods as ScriptHook 1.0 (`AvengersModInit` with
`MatchRva`, `PatchRva` and `FindPattern`), so mods written for ScriptHook's `AvengersModInit` interface
run with it.

## Install
1. Open the game folder (Steam > right-click Marvel's Avengers > Manage > Browse local files).
2. Copy `version.dll` and the `mods` folder from the archive into it (next to `Avengers.exe`).
3. Put `.asi` mods into the `mods` folder.

It writes `mods\ModLoader.log` on every start: which mods were loaded and whether their patches applied.

## Switching from ScriptHook
Copy this `version.dll` over ScriptHook's `version.dll`. `AvengersScriptHook.dll` is then no longer used
and can be deleted. Your `mods` folder stays as it is.

## Steam Deck / Linux (Proton)
Proton uses its own `version.dll` unless told otherwise. In Steam > Marvel's Avengers > Properties >
Launch Options, enter:

    WINEDLLOVERRIDES="version=n,b" %command%

## Troubleshooting
- **Nothing happens / no `mods\ModLoader.log`:** `version.dll` is not next to `Avengers.exe` (check you
  didn't extract into a sub-folder), or your antivirus removed it.
- **A mod logs "wrong game version", or a patch is "not patched":** that mod is made for a different
  game version (these mods are for the Steam version 2.8.2).
- **Disabling a mod:** only files whose name ends exactly in `.asi` are loaded, so `mymod.asi_off` is off.
- **Antivirus warnings:** DLL loaders are sometimes flagged by heuristics. The source code is published
  so you can check it or build it yourself.

## Uninstall
Delete `version.dll` from the game folder (and the `mods` folder if you no longer need it). The game's own
files are never changed.

## For mod authors
`AvengersModInit(const ModApi* api)` receives (0x28 bytes, same layout as ScriptHook 1.0):

| Offset | Field |
|---|---|
| +0x00 | `u32 version` = 0x10000 |
| +0x04 | `u32 size` = 0x28 |
| +0x08 | game module base |
| +0x10 | `int MatchRva(u32 rva, const u8* bytes, u32 len)` |
| +0x18 | `int PatchRva(u32 rva, const u8* expected /*may be NULL*/, const u8* replacement, u32 len)` |
| +0x20 | `void* FindPattern(const u8* bytes, const char* mask /* '?' = any byte */)` |

The same functions are exported as `ASH_GetVersion`, `ASH_MatchRva`, `ASH_PatchRva`, `ASH_FindPattern`.
Mods are loaded from a background thread 0.5 s after start. `PatchRva` does not pause other threads:
patch code that is not running at that moment. Windows' own `version.dll` functions are passed on to
the system's `version.dll`.

## Licence
Free for personal, non-commercial use. Re-uploading or publishing modified versions needs the author's
permission and credit. No commercial use. See LICENSE.
