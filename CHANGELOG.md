# Changelog

## 0.1.2

- Fix an invisible Mods menu after pressing F10. MT2 can leave an offscreen framebuffer bound when presenting the window; framework UI now draws to the window and restores both previous framebuffer bindings afterward. Thanks to Escara for reporting the issue.
- Apply the menu's mouse and text-input state on the first opening in an empty profile, and release it when the menu closes.
- Start the in-game menu and portable helper in English. German remains available through the language button.
- Verify visible output with a separate window capture. An internal framebuffer capture alone could show the menu even when the actual window did not.
- Lua API 1.0.0, native ABI 1 and the Vortex extension remain unchanged.

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
