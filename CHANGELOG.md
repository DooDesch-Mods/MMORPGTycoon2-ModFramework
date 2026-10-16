# Changelog

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
