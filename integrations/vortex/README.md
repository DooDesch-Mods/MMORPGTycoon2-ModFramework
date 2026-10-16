# MMORPG Tycoon 2 - Vortex support

> 🛟 **Need help or found a bug?** Get support at [support.doodesch.de/modframework](https://support.doodesch.de/modframework).

Manage MT2 Framework packages and classic model-only scenery packs through Vortex. This extension discovers Steam app 486860, installs the framework at the game root and deploys framework mods beneath their manifest IDs. Each Vortex profile gets its own framework settings, storage and game data under `<game>/.mt2fw-vortex/profiles/vortex-<profile-id>`. The runtime and mod targets share the game volume, which permits normal Vortex hardlink deployment.

## Install

1. In Vortex **Extensions**, use **Drop File(s)** to install `MMORPGTycoon2-Vortex-0.1.1.zip`, then restart Vortex.
2. Manage **MMORPG Tycoon 2** and select its Steam installation. Deployment checks the exact supported executable; purge remains available after a game update.
3. Import `MT2Framework-0.1.2-windows-x64.zip`, enable it and deploy with the game closed.
4. Import framework mod ZIPs or a supported classic scenery ZIP, enable them and deploy. Start the game through Steam. F10 opens the framework **Mods** menu; classic scenery uses the game's scenery categories.

The framework is a separate **MT2 Framework runtime** mod type. The extension selects **MT2 protected hardlink deployment**, which delegates file tracking and linking to Vortex after its checks pass. Vortex owns deployment and purge of the loader, runtime, tools and generated configuration. No original game archive is replaced. The same packaged CLI validates mods before deployment. Unknown or modified loader files stop deployment and purge with a concrete error.

For authoring, download the separate **MT2Framework-SDK** from GitHub and keep it outside the game folder. Create mods outside Vortex's deployed folders, then check and pack them with the SDK helper/CLI. Import the mod ZIP into Vortex. Use Vortex to enable or remove managed packages; do not install a second manual framework over them. The runtime's small native `mt2fw.exe` provides only the package check used during deployment.

## Classic scenery

Import the original **More Sceneries - Lite Mode 1.3** ZIP directly. Do not add a framework manifest or repack its models. Vortex recognizes the separate **MT2 classic scenery** mod type and retains the original model bytes and names. The user's downloaded package is not included in this extension. Download it from its [author's page](https://www.nexusmods.com/mmorpgtycoon2/mods/2).

Supported classic archives contain one named pack folder with `.vmb` models under `scenery/festival`, `scenery/prop` or `scenery/table`. Readme/license files are allowed. Other payloads are rejected. Model headers and the staged file inventory are checked; two enabled classic packs that provide the same virtual resource produce a named conflict error.

Classic packs use the selected profile's `game-data/<SteamID>/mod/<pack-id>` directory. Start Steam and sign in before the first deployment so the extension can select the account the game will use. An existing single account directory also permits removal when Steam is closed. The framework runtime remains required for profile isolation. Original AppData mods and multiplayer profiles are not moved. Scenery changes require a full game restart.

Classic packs use the game's native loader and save references. They do not gain the framework's content-dependency metadata; keep their original packages with worlds that use them. Validation covers Vortex deployment and native model loading. Interactive catalog selection and placement have not been exercised. See the [scenery acceptance record](https://github.com/DooDesch-Mods/MMORPGTycoon2-ModFramework/blob/main/evidence/vortex-scenery-acceptance.json) for exact checks and limits.

## Existing installations and profiles

Remove a manually installed framework with its original helper before using Vortex. Existing worlds and multiplayer test profiles are not moved. If the helper left its configuration after removal, the extension keeps a byte-for-byte backup in the new profile's `backups/vortex-transition` folder before selecting the Vortex profile.

Switch Vortex profiles and deploy with the game closed. Both mod selection and the framework profile follow the selected Vortex profile. Existing data remains in its original profile. Copy a save explicitly through the helper if you want to reuse it.

## Remove

Disable framework mods and the framework runtime in Vortex, then deploy or purge. Vortex removes only its deployed files. Profile data, settings and saves remain. Custom scenery may be required by a save; keep those content packages available for that save.

## Compatibility

- Windows x64 Steam only. Verified MT2.exe SHA256: `bbab2dc68babcc644d0a1679548bf1e334b6c52aea669c3f61f6cb9d45c1aaf6`.
- The extension targets Vortex 2.8.2 or newer. Native plugins and content changes require a game restart.
- Framework `mod.json` ZIPs, owned framework bundles and the classic model-only layout above are supported. Original archive replacements and the separate MT2 Mod Manager's `manifest.json` format are reported rather than silently converted.
- Multiplayer bridge acceptance and human newcomer timing remain pending.

## Build and checks

```powershell
node --test tests/vortex-routing.cjs
python tools/package_vortex.py
```

The archive has `index.js`, `routing.js`, `guarded-deployment.js`, `steam-account.js`, `info.json`, `gameart.png`, `package.json`, `README.md` and `LICENSE` at its root. The numerical Nexus game ID is 4452, identified by the game's [official tile URL](https://staticdelivery.nexusmods.com/images/games/4_3/tile_4452.jpg). Packaging follows the [official extension specification](https://github.com/Nexus-Mods/Vortex/wiki/How-to-package-a-game-extension). Extension discovery on Nexus is separate from a GitHub build; no Nexus page is automatically created.

Original extension code is MIT. The real-game artwork has separate rights recorded in `media.json` in the source repository.
