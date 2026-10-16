# Skill Unlock 1.0

by Space Trucker · Marvel's Avengers (PC, Steam version 2.8.2) · Requires a mod loader: Avengers
ScriptHook **or** Avengers Mod Loader (either one works).

Adds an **UNLOCK ALL** button to the Character screen. One press (plus a confirm) unlocks everything for
the hero you are playing, the game's own way: the hero is levelled through the game's XP system and every
skill is bought through the game's own purchase code, so the result is saved like normal progress.

## How to use
1. Open the Character screen (press **Esc** / **Start** and pick **CHARACTER**).
2. Under LOADOUT and SKILLS there is a new tile: **UNLOCK ALL**. Select it: it changes to **CONFIRM?**.
3. Select it again within 4 seconds. The hero goes to level 50 and max champion level, and every skill on
   every tab (including champion skills) is bought. The tile then reads **ALL MAXED**.
4. To do another hero, switch to that hero and repeat.

## Install
Install a mod loader first (Avengers ScriptHook or Avengers Mod Loader). Then copy the `mods` folder from
the archive into the game folder (next to `Avengers.exe`).

## Troubleshooting
- **No UNLOCK ALL tile:** check that a mod loader is installed and that `mods\SkillUnlock.log` says
  "ready". If it says "wrong game version" or "code differs", your game version is not supported (Steam
  2.8.2) or another mod changes the same screen.
- The tile uses a free slot in that column. If all four slots are already used, no tile is added.

## Good to know
- It acts on the hero you are currently playing.
- No skill points or champion points are spent. Levelling up grants the normal points, so you keep those.
- Pick-3 skills: all three choices are bought.
- Back up your saves first (`Documents\Marvel's Avengers\<your Steam ID>\`), or use Avengers Save Tool.
- Meant for single player. Using changed progression online is at your own risk.
- If it hits an unexpected error it switches itself off until the game restarts (see `mods\SkillUnlock.log`).

## Uninstall
Delete `mods\SkillUnlock.asi` and `mods\SkillUnlock.log`. Skills you already unlocked stay unlocked (they
are part of your save).

## Licence
Free for personal, non-commercial use. Re-uploading or publishing modified versions needs the author's
permission and credit. No commercial use. See LICENSE.
