# Collection Unlock 1.0

by Space Trucker · Marvel's Avengers (PC, Steam version 2.8.2) · Requires a mod loader: Avengers
ScriptHook **or** Avengers Mod Loader (either one works).

Adds a **COLLECTION** button to the Character screen. One press (plus a confirm) gives you everything that
is still locked, for every hero:
- **outfits**, with all of their colour and style options
- **emotes**, **takedowns** and **nameplates**
- **collectibles**: comics (raised to their top tier, for the biggest bonus), intel files (audio logs, chats, classified files, emails, news) and codex entries

The final update (2.8) made most Marketplace cosmetics free but kept some locked, such as campaign
rewards and cosmetic vendor items. Collectibles are normally found one by one in missions. This mod hands
all of them out through the game's own reward and pickup code, so they are saved like items you earned,
and any rewards linked to them are given too. No popups are shown.

It also unlocks the final update's Founder's Gift: Iron Man's **Variable Threat Response** suit (the
"War Machine" armor). The game only gives it to players with a Steam achievement earned before April 2023,
which an offline copy can't show, so it stays under LOCKED. The tile unlocks it the way the game stores an
outfit you have used, so it stays unlocked (also if you remove the mod).

Not touched: Spider-Man's outfits (PlayStation only, the hero isn't playable on PC) and a few outfit styles
the game marks as unreleased. They are left as they are and not counted.

## How to use
1. Open the Character screen (press **Esc** / **Start** and pick **CHARACTER**).
2. In the left column there is a new tile: **COLLECTION**. It sits under Skill Unlock's **UNLOCK ALL** if
   you use that mod, or under **SKILLS** if you don't. Select it: it changes to **CONFIRM?**.
3. Select it again within 4 seconds. Everything is given at once, for all heroes. The tile then reads
   **ALL OWNED**.
4. Pick outfits, emotes, takedowns and nameplates in each hero's Appearance screen. Comics, intel and the
   codex are in the Collections screen.

## Install
Install a mod loader first (Avengers ScriptHook or Avengers Mod Loader). Then copy the `mods` folder from
the archive into the game folder (next to `Avengers.exe`).

Using it with Skill Unlock: use Skill Unlock **1.1 or newer**. Version 1.0 was not made to share the column
and may not install next to it.

## Troubleshooting
- **No COLLECTION tile:** check that a mod loader is installed and that `mods\CollectionUnlock.log` says
  "ready". If it says "wrong game version" or "code differs", your game version is not supported (Steam
  2.8.2) or another mod changes the same screen.
- The tile uses a free slot in that column (it has four). If only one slot is free, Skill Unlock's UNLOCK ALL
  gets it and no COLLECTION tile is added.
- `mods\CollectionUnlock.log` lists, for each type, how many items were locked and how many were given.

## Good to know
- Everything you unlock is part of your save. Back up your saves first
  (`Documents\Marvel's Avengers\<your Steam ID>\`), or use Avengers Save Tool.
- Collecting everything can complete the game's own collection achievements.
- Newly given items may show the game's "new" marker until you look at them.
- Meant for single player. Using unlocked items online is at your own risk.
- If it hits an unexpected error it switches itself off until the game restarts (see the log).

## Uninstall
Delete `mods\CollectionUnlock.asi` and `mods\CollectionUnlock.log`. Everything you already unlocked stays
unlocked (it is part of your save).

## Licence
Free for personal, non-commercial use. Re-uploading or publishing modified versions needs the author's
permission and credit. No commercial use. See LICENSE.
