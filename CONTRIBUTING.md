# Contributing

> 🛟 **Need help or found a bug?** Get support at [support.doodesch.de/modframework](https://support.doodesch.de/modframework).

Help modders make something useful. A good issue describes the intended result, the framework/game version, a small reproduction and the original error. Remove private paths, account details and save data before sharing a diagnostic report.

Start with the [build guide](docs/contributing/build.md). Source lives in `src/core`, `src/loader` and `adapters/mt2`. Public native declarations are in `include/mt2fw/plugin.h`; Lua contracts are in `api/contract.json`.

[Nexus release automation](integrations/nexus/README.md) documents the two upload destinations, existing-version checks and manual validation runs. Published documentation is deployed by the separate **Publish documentation** workflow.

The package build produces two downloads: `MT2Framework-<version>-windows-x64.zip` for players and `MT2Framework-SDK-<version>-windows-x64.zip` for modders. Only the runtime goes to Nexus. The SDK stays on GitHub and includes its matching runtime under `runtime/`. Runtime files are allowlisted and verified again before Nexus upload; Python, authoring tools and offline documentation cannot enter the player package. Vortex uses the native runtime validator at the existing `mt2fw/mt2fw.exe check` command. The full SDK CLI and helper retain their shared Python validation.

## API and example changes

1. Update the public contract and the corresponding implementation or complete example.
2. Run `python tools/generate_docs.py`.
3. Run `python tools/generate_docs.py --check` and `python tools/check_contract.py` after building the probe.
4. Add a migration note for a semantic change. A generated signature diff cannot detect changed meaning.

Never update the 1.0.0 baseline merely to hide a breaking change. Add a new versioned baseline when preparing an accepted API release. Generated outputs are checked in so documentation and types are visible without build tools.

## Documentation and images

Public roadmap stages and feature coverage come from `docs/roadmap.json`. Update the released version and only mark features available when they are included in that download. Run `python tools/render_roadmap.py` to refresh the README, text roadmap and desktop/mobile SVGs. CI runs the same command with `--check` to prevent stale copies. Coverage percentages count each listed area once, without crediting partial or experimental areas as fully available. Keep planned work separate from released behavior and omit internal test records and date promises.

Author guides in English Markdown. Keep each tutorial step small and show the actual result. Use existing captured images if their workflow is unchanged. Update `docs/media.json` when a pictured interface changes. Keep the original capture separate from optional editable annotation and export.

Run `npm run docs:build`. It produces the browser site and a directly openable offline copy. `npm run docs:test` checks rendered views, navigation, search, code copying and offline use. Browser tests require Playwright Chromium or `CHROMIUM_EXECUTABLE` pointing to an existing compatible executable.

Use the original brand SVGs in `assets/brand` and the documented tokens in `docs/theme`. Game imagery has separate rights; the private game-icon reference is not part of this repository.

## License and scope

Contribute original work under this repository's MIT license. Keep third-party notices with copied dependencies. The provenance inventory covers explicitly extracted authored copies, not the entire multiplayer product. This source tree does not include the game's original files.

Checks for an unchanged path remain closed. Documentation edits alone do not require another game run. Source checks, a synthetic-host example test and actual game acceptance are distinct results.
