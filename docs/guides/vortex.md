---
title: Install with Vortex
---

# Install with Vortex

The [MMORPG Tycoon 2 Vortex Extension](https://www.nexusmods.com/site/mods/2465) installs the framework and its mods through Vortex. Each Vortex profile keeps separate settings, mod data and worlds.

## Requirements

- Windows x64 and Vortex 2.8.2 or newer.
- The [supported Steam game build](../compatibility.md).
- The full framework ZIP from [Nexus](https://www.nexusmods.com/mmorpgtycoon2/mods/25) or [GitHub Releases](https://github.com/DooDesch-Mods/MMORPGTycoon2-ModFramework/releases/tag/v0.1.0).
- A staging folder on the same volume as the game.

## Install

1. Close the game. Remove an existing manual framework installation with its original helper.
2. Open **Extensions** in Vortex, drop in the extension ZIP and restart Vortex.
3. Manage **MMORPG Tycoon 2** and select the Steam installation.
4. Import the full framework ZIP as a mod, enable it and deploy.
5. Import and enable your mod ZIPs, deploy, then start the game through Steam. **F10** opens Mods.

Vortex manages deployment and removal for this installation. Keep the game closed while switching profiles or changing deployment. Original game archives are not replaced. Unknown loaders or modified managed files produce a named conflict.

## Classic scenery

The extension also recognizes model-only scenery archives, including the original [More Sceneries - Lite Mode](https://www.nexusmods.com/mmorpgtycoon2/mods/2) download by Cow. Download such packs from their authors; the extension does not bundle them.

Supported archives have one named pack folder containing `.vmb` models under `scenery/festival`, `scenery/prop` or `scenery/table`. Import the original ZIP without adding `mod.json`. Conflicting resource names are reported before deployment.

Sign in to Steam before the first classic deployment. Models are placed in the selected Vortex profile and Steam account's game-data directory. Restart the game after scenery changes, and keep the packs required by your worlds. Classic packs use native save references without the framework's missing-content metadata.

Deployment, profile switching, disable and purge have been checked with the original More Sceneries Lite 1.3 ZIP. The engine loaded one representative model. Interactive catalog placement and saving/reloading a placed classic object remain unverified; see the [acceptance record](https://github.com/DooDesch-Mods/MMORPGTycoon2-ModFramework/blob/main/evidence/vortex-scenery-acceptance.json).

Legacy MT2 Mod Manager `manifest.json` merging and replacements for original game archives are not supported. Multiplayer compatibility is not confirmed.

## Remove

Close the game, purge deployment and remove the managed packages through Vortex. Profile data and saves remain available. Remove the extension itself after purging, then restart Vortex.

For development, keep source projects outside Vortex's managed folders. Check and pack a mod with the helper or CLI, then import that ZIP into Vortex.
