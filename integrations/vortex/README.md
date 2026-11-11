# MMORPG Tycoon 2 Vortex Extension

> 🛟 **Need help or found a bug?** Get support at [support.doodesch.de/modframework](https://support.doodesch.de/modframework).

Adds MMORPG Tycoon 2 to Vortex. Vortex finds the Steam installation and deploys with its own hardlink deployment. One installer places every archive:

| Archive contains | Installed to |
| --- | --- |
| `framework-package.json` (MT2 Mod Framework Runtime) | the game folder, plus a generated `mt2fw.json` that points the framework at `<game>\mt2fw-profile` |
| `mod.json` (framework mod) | `<game>\mt2fw-profile\mods\<id>\` |
| anything else (classic mod) | `<game>\mt2fw-profile\game-data\<SteamID64>\mod\<folder>\` |

All Vortex profiles share the one framework profile `<game>\mt2fw-profile`. Your worlds and settings are stored there in `game-data`.

Every install step and every failure is written to the Vortex log with the prefix `MT2:`.

## Classic mods

Classic mods are mods without a `mod.json`. They need the Framework Runtime, because the runtime points the game's data folder to `mt2fw-profile\game-data`.

- If the archive starts with a game data folder (`buildings`, `building_definitions`, `building_themes`, `icons`, `materials`, `scenery`, `wall_themes`), its content goes into a folder named after the archive.
- Otherwise each top-level folder is a mod folder. Loose files at the top, such as readmes, are skipped.
- The SteamID64 comes from the signed-in Steam user. With Steam closed, the extension uses the single account folder in `mt2fw-profile\game-data` or `%APPDATA%\VectorStorm\MMORPG Tycoon 2`. With none or several, the install fails; sign in to Steam and try again.

MT2 Mod Manager packages (`manifest.json`), character cards and tools are not recognized; the installer treats them as classic mods. Install those by hand.

## Install

1. In Vortex **Extensions**, use **Drop File(s)** to install `MMORPGTycoon2-Vortex-0.2.0.zip`, then restart Vortex.
2. Manage **MMORPG Tycoon 2**. The staging folder must be on the same drive as the game.
3. Install the **MT2 Mod Framework Runtime** ZIP, then your mod ZIPs. Enable them and deploy with the game closed.
4. Start the game through Steam. F10 opens the framework **Mods** menu.

## Update from 0.1.x

Version 0.2.0 drops the protected copy deployment and the per-profile framework folders.

1. Before you update the extension, purge in Vortex (**Mods** > **Purge**).
2. If you updated without purging: close the game and delete `WSOCK32.dll`, `MT2Framework.dll`, `mt2fw.json`, `framework-package.json`, the `mt2fw` folder, `.mt2fw-vortex-owner.json` and `.mt2fw-copy-deployment.json` from the game folder. Then deploy.
3. To keep your worlds, copy `<game>\.mt2fw-vortex\profiles\vortex-<profile-id>\game-data` to `<game>\mt2fw-profile\game-data`.
4. Reinstall classic scenery packs; the old classic scenery mod type no longer exists.

## Remove

Disable the mods and purge in Vortex. Vortex removes the files it deployed. `<game>\mt2fw-profile\game-data` keeps your worlds and settings.

## Build and checks

```powershell
node --test tests/vortex.cjs
python tools/package_vortex.py
```

The archive has `index.js`, `info.json`, `gameart.png`, `package.json`, `README.md` and `LICENSE` at its root. Steam app ID: 486860. Nexus game ID: 4452.

Original extension code is MIT. The real-game artwork has separate rights recorded in `media.json` in the source repository.
