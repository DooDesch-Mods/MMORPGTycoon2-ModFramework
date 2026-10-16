# MT2 Mod Framework Runtime

> 🛟 **Need help or found a bug?** Get support at [support.doodesch.de/modframework](https://support.doodesch.de/modframework).

Run Lua mods, native plugins and custom scenery in the supported Windows Steam version of MMORPG Tycoon 2. This player download contains the loader, framework, non-executing mod validator and required notices. Python and developer tools are not required.

## Install with Vortex

1. Install the [MMORPG Tycoon 2 Vortex Extension](https://www.nexusmods.com/site/mods/2465) and restart Vortex.
2. Manage MMORPG Tycoon 2 and select the Steam installation.
3. Import this runtime ZIP, enable it and deploy with the game closed.
4. Install your mods through Vortex, enable them and deploy.
5. Start through Steam. Open **Mods** on the game's desktop, or press F10.

Keep one enabled framework version. Vortex provides separate profiles and checks the supported game build, file ownership and mod packages. Use Vortex to update, disable or remove its managed runtime. Existing saves and multiplayer test profiles are not imported automatically.

## Manual installation

For a new installation with no existing loader:

1. Close the game. Extract the ZIP outside the Steam game folder.
2. Copy `WSOCK32.dll`, `MT2Framework.dll` and the `mt2fw` folder beside `MT2.exe`. Preserve any existing loader; do not overwrite unknown files.
3. Create `mt2fw.json` beside `MT2.exe` with an absolute path to a new, empty mod profile:

```json
{"enabled": true, "profile": "C:/MT2Mods/default"}
```

The framework creates the profile folders on startup. Extract each framework mod so its `mod.json` is under `<profile>/mods/<mod-id>/`. Start through Steam. The profile has its own game data; copy existing saves only through the SDK helper's explicit import action if needed.

To update a previous helper installation or switch to Vortex, first remove it with its original helper. Keep the profile and original game files. For a manual runtime update, replace only files listed in its `framework-package.json` with the game closed. To remove a manual runtime, remove those package files; retain `mt2fw.json` and the profile to keep settings and saves.

## Creating mods

The separate **MT2Framework-SDK** download on [GitHub Releases](https://github.com/DooDesch-Mods/MMORPGTycoon2-ModFramework/releases) contains the portable helper, full CLI, GLB importer, templates, editor types, examples and offline documentation. It also includes the runtime under `runtime/`. Keep the SDK outside the game folder.

The runtime's `mt2fw/mt2fw.exe check <mod-directory>` only validates packages. It does not execute a mod or open a GUI. Vortex uses this command before deployment.

## Compatibility and diagnostics

Only the verified Windows x64 Steam build is supported. The adapter stays disabled for other game builds. Multiplayer compatibility is not established. See the [compatibility guide](https://doodesch-mods.github.io/MMORPGTycoon2-ModFramework/compatibility.html).

Read `<profile>/bootstrap.jsonl` and `<profile>/logs/framework.jsonl` for diagnostics. Native plugins and content changes require a restart; Lua mods can be reloaded from **Mods**.

Original framework code is MIT licensed. Keep `LICENSE`, `THIRD_PARTY_NOTICES.md` and `licenses/` with the runtime. No original game files are included.
