# AvengersGame-tools

Mods and tools for **Marvel's Avengers (PC, Steam version 2.8.2)** by Space Trucker.

| Tool | Version | What it does | Docs |
|---|---|---|---|
| **Avengers Mod Loader** | 1.1 | Loads `.asi` mods from the game's `mods` folder (a `version.dll` loader). Runs mods written for Avengers ScriptHook's interface. Use it *or* ScriptHook, not both. | [docs/ModLoader.md](docs/ModLoader.md) |
| **Skill Unlock** | 1.1 | An **UNLOCK ALL** button on the Character screen: levels the current hero to 50 and max champion level, and buys every skill through the game's own code. Needs a mod loader (ScriptHook or Avengers Mod Loader). | [docs/SkillUnlock.md](docs/SkillUnlock.md) |
| **Collection Unlock** | 1.0 | A **COLLECTION** button on the Character screen (under Skill Unlock's): gives every locked outfit and outfit style, emote, takedown and nameplate, and every comic (at its top tier), intel file and codex entry, through the game's own reward and pickup code, plus the final update's Founder's Gift (Iron Man's Variable Threat / War Machine suit). Needs a mod loader. | [docs/CollectionUnlock.md](docs/CollectionUnlock.md) |
| **Quick Start** | 1.0 | Keeps the game loading when its window is in the background, and skips the startup screens (warning, legal wait, intro movie, press any key) straight to the main menu. Each part can be switched off. Needs a mod loader. | [docs/QuickStart.md](docs/QuickStart.md) |
| **Avengers Save Tool** | 1.0 | Windows tool: unlock the war table and all heroes without the campaign, import a save shared by another player, back up / restore. | [docs/SaveTool.md](docs/SaveTool.md) |

## Download
Ready-to-use archives are on the [Releases](https://github.com/Space-Trucker/AvengersGame-tools/releases)
page and on the author's Nexus Mods pages. Each archive extracts straight into the game folder (the Save
Tool runs from anywhere). Install steps are in each tool's docs.

## Repository layout
```
src/ModLoader/      version.dll mod loader (C++)
src/SkillUnlock/    SkillUnlock.asi (C++)
src/CollectionUnlock/ CollectionUnlock.asi (C++)
src/QuickStart/      QuickStart.asi + QuickStart.ini (C++)
src/Common/          code shared by the Character screen tile mods
src/SaveTool/       Avengers Save Tool (C#, .NET Framework 4.x)
docs/               user docs per tool, save file format
scripts/            build.bat (compile everything to out/), package.ps1 (release archives to dist/)
.github/            CI build on every push, issue templates
```

## Build from source
1. Install Visual Studio 2019/2022 (any edition, or Build Tools) with the C++ x64 tools.
2. Run `scripts\build.bat`. The files land in `out\`. Warnings are treated as errors.
3. Optional: `powershell -ExecutionPolicy Bypass -File scripts\package.ps1` makes the release archives
   in `dist\`.

The save tool is built with the C# compiler that ships with Windows, so it needs nothing extra.

## Notes
- Single player. Back up your saves. Using changed progression online is at your own risk.
- Not affiliated with or endorsed by Marvel, Square Enix or Crystal Dynamics.
- Changes: [CHANGELOG.md](CHANGELOG.md) · Security: [SECURITY.md](SECURITY.md) ·
  Contributing: [CONTRIBUTING.md](CONTRIBUTING.md)

## Licence
Free to use and the source is public.

- **You can** use the mods for free, read the source and build it yourself.
- **Re-uploads and modified versions:** ask first and credit Space Trucker.
- **No commercial use:** don't sell them, put them behind a paywall or use them in a paid product.

Full terms in [LICENSE](LICENSE).
