# Contributing

> 🛟 **Need help or found a bug?** Get support at [support.doodesch.de/modframework](https://support.doodesch.de/modframework).

Help modders make something useful. A good issue describes the intended result, the framework/game version, a small reproduction and the original error. Remove private paths, account details and save data before sharing a diagnostic report.

Start with the [build guide](docs/contributing/build.md). Source lives in `src/core`, `src/loader` and `adapters/mt2`. Public native declarations are in `include/mt2fw/plugin.h`; Lua contracts are in `api/contract.json`.

## API and example changes

1. Update the public contract and the corresponding implementation or complete example.
2. Run `python tools/generate_docs.py`.
3. Run `python tools/generate_docs.py --check` and `python tools/check_contract.py` after building the probe.
4. Add a migration note for a semantic change. A generated signature diff cannot detect changed meaning.

Never update the 1.0.0 baseline merely to hide a breaking change. Add a new versioned baseline when preparing an accepted API release. Generated outputs are checked in so documentation and types are visible without build tools.

## Documentation and images

Author guides in English Markdown. Keep each tutorial step small and show the actual result. Use existing captured images if their workflow is unchanged. Update `docs/media.json` when a pictured interface changes. Keep the original capture separate from optional editable annotation and export.

Run `npm run docs:build`. It produces the browser site and a directly openable offline copy. `npm run docs:test` checks rendered views, navigation, search, code copying and offline use. Browser tests require Playwright Chromium or `CHROMIUM_EXECUTABLE` pointing to an existing compatible executable.

Use the original brand SVGs in `assets/brand` and the documented tokens in `docs/theme`. Game imagery has separate rights; the private game-icon reference is not part of this repository.

## License and scope

Contribute original work under this repository's MIT license. Keep third-party notices with copied dependencies. The provenance inventory covers explicitly extracted authored copies, not the entire multiplayer product. This source tree does not include the game's original files.

Checks for an unchanged path remain closed. Documentation edits alone do not require another game run. Source checks, a synthetic-host example test and actual game acceptance are distinct results.
