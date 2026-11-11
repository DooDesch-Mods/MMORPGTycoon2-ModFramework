---
title: Install with Vortex
---

# Install with Vortex

The [MMORPG Tycoon 2 Vortex Extension](https://www.nexusmods.com/site/mods/2465) installs the framework, framework mods and classic mods through Vortex's own hardlink deployment. Its source, releases and issues live in [DooDesch-Mods/MMORPGTycoon2-Vortex](https://github.com/DooDesch-Mods/MMORPGTycoon2-Vortex).

## Requirements

- Windows x64 and Vortex 2.8.2 or newer.
- The [supported Steam game build](../compatibility.md).
- The player runtime ZIP from [Nexus](https://www.nexusmods.com/mmorpgtycoon2/mods/25) or [GitHub Releases](https://github.com/DooDesch-Mods/MMORPGTycoon2-ModFramework/releases).
- A staging folder on the same drive as the game.

## Install

1. Close the game. Remove an existing manual framework installation with its original helper.
2. Open **Extensions** in Vortex, drop in the extension ZIP from Nexus or the [GitHub releases](https://github.com/DooDesch-Mods/MMORPGTycoon2-Vortex/releases) and restart Vortex.
3. Manage **MMORPG Tycoon 2** and select the Steam installation.
4. Install the player runtime ZIP as a mod, enable it and deploy. The developer SDK is a separate GitHub download and stays outside the game folder.
5. Install and enable your mod ZIPs, deploy, then start the game through Steam. **F10** opens Mods.

## Where files go

| Archive contains | Installed to |
| --- | --- |
| `framework-package.json` (player runtime) | the game folder, plus `mt2fw.json` pointing at `<game>\mt2fw-profile` |
| `mod.json` (framework mod) | `<game>\mt2fw-profile\mods\<id>\` |
| anything else (classic mod) | `<game>\mt2fw-profile\game-data\<SteamID64>\mod\<folder>\` |

All Vortex profiles share `<game>\mt2fw-profile`. Worlds and settings are stored in its `game-data` folder.

`mt2fw.json` holds the full path of the profile. After you move the game folder, set the new location in Vortex, then reinstall the runtime mod (right-click > **Reinstall**) and deploy.

Classic mods load only while the player runtime is enabled and deployed, because the runtime points the game's data folder to `mt2fw-profile\game-data`. Without it, the extension logs a warning and shows a Vortex notification. If the runtime rejects a new game build, the game starts without mods and uses its normal AppData folder: classic mods and the worlds in `mt2fw-profile\game-data` stay on disk but are not loaded until a runtime update supports that build. An archive that starts with a game data folder such as `buildings`, `icons`, `materials` or `scenery` is placed in a folder named after the archive. Otherwise each top-level folder is a mod folder and loose readme files are skipped. Sign in to Steam before installing a classic mod, so the extension can pick your account folder.

MT2 Mod Manager packages (`manifest.json`), character cards and tools are not recognized. Install those by hand.

Every install step and every failure is written to the Vortex log with the prefix `MT2:`.

## Update from extension 0.1.x

1. Before you update the extension, purge in Vortex.
2. If you updated without purging: close the game and delete `WSOCK32.dll`, `MT2Framework.dll`, `mt2fw.json`, `framework-package.json`, the `mt2fw` folder, `.mt2fw-vortex-owner.json` and `.mt2fw-copy-deployment.json` from the game folder. Then deploy.
3. To keep your worlds, copy `<game>\.mt2fw-vortex\profiles\vortex-<profile-id>\game-data` to `<game>\mt2fw-profile\game-data`.
4. Reinstall classic scenery packs.

## Remove

Close the game, disable the mods and purge in Vortex. Your worlds and settings stay in `<game>\mt2fw-profile\game-data`. Remove the extension itself after purging, then restart Vortex.

For development, keep source projects outside Vortex's managed folders. Check and pack a mod with the helper or CLI, then install that ZIP in Vortex.
