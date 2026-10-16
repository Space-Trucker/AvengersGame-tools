# Avengers Save Tool 1.0

by Space Trucker · Marvel's Avengers (PC, Steam version 2.8.2) · Windows only · no mod loader needed,
nothing is installed.

## What it does
- **Unlock war table + heroes:** sets the story progress of a save so the Helicarrier war table
  (Avengers Initiative) and all heroes are available, without playing the Reassemble campaign.
- **Import a shared save:** saves are encrypted per Steam account, which is why a save from someone else
  shows "corrupted data". The tool re-encrypts it for your account so it loads.
- **Back up / restore:** every change first copies the account's save files (`.dat`) to
  `Documents\Marvel's Avengers\SaveTool Backups\`.

## How to use
1. **Close the game completely.** (The game writes its saves when it exits, so the tool only changes
   saves while the game is closed.)
2. Run `AvengersSaveTool.exe`. It finds your save folder (`Documents\Marvel's Avengers\<Steam ID>\`).
3. **Unlock:** click "Unlock war table + heroes". With nothing selected in the list, every save is
   unlocked (simplest); or select only the saves you want.
   **Starting fresh?** Start a new game and choose Avengers Initiative. As soon as you are in the game it
   has made its first save: close the game, run the tool, unlock, then start the game again and
   continue. The war table and all heroes are now available.
4. **Import:** click "Import a shared save..." and pick the folder with the other player's save files.
   If that folder isn't named with their Steam ID, the tool asks for it. Your settings (`profile1.dat`)
   are kept; your current saves are backed up first.
5. Start the game and load the save.

**Restore a backup:** close the game, click "Restore a backup..." and pick the folder named with your
Steam ID inside one of the dated folders in `Documents\Marvel's Avengers\SaveTool Backups\`. Your
current saves are backed up first, so a restore can itself be undone.

## Steam Cloud
Steam Cloud can bring old saves back. If Steam later shows a Cloud conflict, choose the files on this PC
(local). Or turn Steam Cloud off for the game (Steam > Properties > General).

## Troubleshooting
- **"Windows protected your PC" (SmartScreen):** the tool is not code-signed. Click "More info" >
  "Run anyway". The source code is published.
- **"Please close Marvel's Avengers first":** the game is still running; quit it completely.
- **"No save folder found":** start the game once and let it make a save. If your Documents folder is
  redirected (for example to OneDrive), the tool uses Windows' real Documents location, like the game.
- **"can't read (not a save for this account?)":** the file was made under another Steam account; use
  "Import a shared save" instead.

## Notes
- Needs .NET Framework 4.x, which is part of Windows 10 and 11.
- Unlocking changes one story value in the save; nothing else in the save's data changes. (The file is
  re-compressed and re-encrypted, so its bytes on disk look completely different, which is normal.)
- Meant for your own saves and single player. Using changed progression online is at your own risk.
- Technical details: [save-format.md](save-format.md).

## Uninstall
Delete `AvengersSaveTool.exe`. Backups stay in `Documents\Marvel's Avengers\SaveTool Backups\` until you
delete them.

## Licence
Free for personal, non-commercial use. Re-uploading or publishing modified versions needs the author's
permission and credit. No commercial use. See LICENSE.
