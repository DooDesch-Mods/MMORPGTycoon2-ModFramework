# Changelog

## 0.3.0

### Added

- Build character editors, Dungeon tools and management mods with Lua.
- Place and edit modular buildings, scenery, paths, walls and terrain through the game's native tools.
- Create NPCs, Monsters and Bosses; edit quests, Boss phases and Monster Zones.
- Manage staff, research projects, server networks, fees, loans, marketing and game releases.
- Use typed editor completion, generated function pages and a machine contract for the new gameplay APIs.
- Browse the available gameplay operations with the complete Gameplay API Explorer example.

### Changed

- See the result of your world edits, with clear failure causes when the game cannot confirm a change.
- Reloading or disabling a Lua mod cancels its old pending world edits. Old references cannot affect a newly loaded world.

### Compatibility

- These gameplay APIs are experimental. Their world-changing operations currently run in singleplayer; a `shared` manifest does not synchronize them in Multiplayer.

## 0.2.6

### Added

- Support the shared right-click block tool used by Multiplayer Early Beta 18.

### Changed

- Extend native input diagnostics to handle the right mouse button.


## 0.2.5

### Updated

- Add the matching native support for Multiplayer Early Beta 17, including shared scenery, groups and building history.
- Keep existing Lua mods and native plugins compatible.

### Fixed

- Deliver diagnostic mouse presses and releases in separate game updates so native game buttons can receive a complete click.
- Keep diagnostic clicks tied to the intended game window and cursor entry state.

### Install

Close the game and update the Framework Runtime. Players only need the Runtime ZIP; the separate SDK on GitHub is for creating mods.

## 0.2.4

### Added

- Open Lua mod apps from Mods, with an optional game-desktop icon and no extra function key.
- Create native Lua windows with text fields, buttons, tabs, panels and PNG images.
- Build searchable lists and flexible layouts with the document renderer.
- Read building types and transforms, region ownership, finances, rating and fun from Lua.
- Three complete Lua examples, with updated editor types and API documentation.

### Fixed

- Reloading a Lua app with a desktop icon no longer triggers a duplicate-icon game assertion.

### Compatibility

- New functions require Lua API 1.1.0. Existing API 1.0 mods remain supported. Native ABI 1 is unchanged; UI operations remain experimental.

## 0.2.3

### Added

- Identify modded sessions in the game's own log, helping VectorStorm and mod authors investigate bug and crash reports.
- List each mod's ID, version and type before it runs, followed by its activation status.

## 0.2.2

### Changed

- Updated for the next Multiplayer Early Beta.

### Fixed

- Prevent some multiplayer edits from being blocked when several mod features are active.

## 0.2.1

### Added

- Add the named Monster placement route used by the next Multiplayer Early Beta. The Framework keeps hook ownership and exact game-version checks in one place.

### Compatibility

- Lua API 1.0.0 and native ABI 1 remain unchanged. Existing Lua mods and native plugin interfaces keep their contracts.

## 0.2.0

### Added

- Experimental patches for Lua and C++ plugins. Run Before/After callbacks around game updates and object creation through named targets.
- One shared callback order across Lua and native plugins, with priorities, explicit ordering rules and readable conflict diagnostics.
- A typed time policy for Framework requests, with one writer per target and visible rejection reasons.
- Complete Lua observer, time-policy and native C++ examples, with editor types and an agent-readable patch contract.

### Changed

- Lua reload validates the replacement patch set before switching. A rejected replacement keeps the previous mod and callback order active.
- Empty Update targets skip patch payload construction when no mod subscribes.

### Compatibility

- Lua API 1.0.0 and native ABI 1 remain unchanged. The patch extension is experimental and requires Framework 0.2.0.
- Time policies require a proven standalone session. They affect Framework requests; native game clock controls keep their behavior.

## 0.1.6

### Added

- Native mod apps can add their own icon to the game desktop, with a separate launch request for each app.
- Native mod apps can open the original New Game wizard and observe its state. An already open wizard keeps the player's name, map seed and current page.
- Experimental app integration guide and machine contract for native mod authors.

### Compatibility

- Multiplayer uses the new app operations and requires this Runtime or newer. Lua API 1.0.0 and native C ABI 1 remain unchanged.

## 0.1.5

### Added

- Support for the optional MT2 Profiler mod, with bounded native captures and exported reports.
- Versioned experimental native services for instrumented sections, shared event routes and owned hooks.

### Fixed

- Native mod windows can be opened again after closing them with the game's X button.
- The game's first world snapshot receives explicit pre-ready metadata, separate from normal save confirmations.

## 0.1.4

### Added

- Open Mods... from the in-game menu, just above Options....
- Launch mod apps from their entry in Mods. Each mod can have its own Open button without needing a separate function key.
- Native blue game windows, text fields, buttons and image previews for mod authors. MT2 Card Vault is the first mod to use them.

## 0.1.3

### Added

- Open Mods from its new icon on the game desktop, or press F10. The native window puts installed mods, settings and Lua reload in one place.
- See reload errors with their file and line directly in Mods. A failed replacement keeps the previous instance running.

### Changed

- A cleaner Mods window with a dark theme, clear selection and mod details. Mod windows stay out of the way while you manage your mods.

### Fixed

- Machine-contract and schema links now work when reading the documentation on GitHub, on the website and offline.

## 0.1.2

### Fixed

- The Mods menu now appears when you press F10. Thanks to Escara for reporting the issue.
- Opening Mods for the first time captures mouse and text input correctly. Closing it returns control to the game.

### Changed

- The menu and portable helper now start in English. German is still available through the language button.

## 0.1.1

- Separate the player runtime from the developer SDK. Nexus distributes only the runtime; the complete SDK remains a separate GitHub download.
- Replace the bundled Python CLI in the runtime with a small native package validator. Existing Vortex deployment keeps the same check command.
- Keep the portable helper, full CLI, GLB importer, examples and offline documentation in the SDK. The helper installs its bundled runtime without deploying developer files.
- Validate the runtime file inventory before packaging and before Nexus upload. Lua API 1.0.0 and native ABI 1 are unchanged.


## 0.1.0 development preview

- Standalone core, loader and MT2 adapter source with vendored native dependencies.
- Separate Lua states, mod-owned subscriptions and transactional script replacement.
- Native C ABI 1, native world actions and static GLB scenery conversion.
- Portable German/English helper, CLI, Lua and C++ templates.
- Original icon and documentation theme, illustrated guides and offline documentation.
- One versioned Lua API contract generates editor definitions, reference and agent-readable data.

Published as [v0.1.0](https://github.com/DooDesch-Mods/MMORPGTycoon2-ModFramework/releases/tag/v0.1.0), with framework 0.1.0 and separate Vortex extension 0.1.1. The tag identifies the exact accepted package source. Multiplayer bridge acceptance and human newcomer timing remain pending.

## Vortex extension 0.1.1

- Install original model-only classic scenery ZIPs, including More Sceneries - Lite Mode 1.3.
- Preserve original model bytes and scope deployment to the selected Vortex profile and Steam account.
- Validate model headers, staged inventories and competing virtual resources before deployment.
- Detect scenery type before activation and keep scenery folders out of the framework mod list.
- Retain the exact [deployment and native model-loading acceptance limits](evidence/vortex-scenery-acceptance.json).
