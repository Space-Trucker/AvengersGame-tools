# Quick Start 1.0

by Space Trucker · Marvel's Avengers (PC, Steam version 2.8.2) · Requires a mod loader: Avengers
ScriptHook **or** Avengers Mod Loader (either one works).

Gets you into the game faster:
- **No pause in the background.** The game normally stops loading (and freezes) while its window is not
  in front. With Quick Start it keeps loading and running, so you can do something else meanwhile.
- **Skips the startup screens:** the photosensitivity warning, the legal screen's wait, the intro movie
  and "press any key". Your profile and save load the normal way, then the main menu opens. Press
  **Continue** to play.

## Install
Install a mod loader first (Avengers ScriptHook or Avengers Mod Loader). Then copy the `mods` folder from
the archive into the game folder (next to `Avengers.exe`).

## Settings
`mods\QuickStart.ini`: set a line to `0` to switch that part off, then restart the game.

| Setting | What it does |
|---|---|
| `NoPauseWhenUnfocused` | keep loading and running when the window is not in front |
| `SkipEpilepsyWarning` | skip the photosensitivity warning screen |
| `SkipLegalWait` | don't wait 3 seconds on the legal screen |
| `SkipIntroMovie` | skip the intro movie |
| `AutoPressStart` | press "any key" on the title screen for you |

## Good to know
- With the pause removed, the game also keeps running in a mission when you switch to another window.
  Pause the game first if you need to.
- Keyboard and mouse are not read while the window is in the background; a controller may still be.
- Use borderless or windowed mode for switching windows; exclusive full screen may still minimize.
- If menus look unfinished right after start (on a very slow drive), set `SkipLegalWait=0`.
- `mods\QuickStart.log` lists what was applied. If the game's code is not what Quick Start expects (another
  game version or another mod), that part is skipped and logged.

## Uninstall
Delete `mods\QuickStart.asi`, `mods\QuickStart.ini` and `mods\QuickStart.log`. Nothing in your save or the
game files is changed.

## Licence
Free for personal, non-commercial use. Re-uploading or publishing modified versions needs the author's
permission and credit. No commercial use. See LICENSE.
