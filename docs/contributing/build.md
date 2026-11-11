---
title: Build and contribute
---

# Build and contribute

Use Windows x64, Visual Studio 2022 Desktop development with C++, CMake 3.24 or newer, Python 3.11 or newer, and Node 22 or newer for documentation. The repository vendors native dependencies, including pinned Lua 5.4.9 source, and needs no multiplayer checkout or installed game to compile and run core/example checks.

```powershell
python tools/build.py --check
python tools/build.py --smoke
```

`--check` builds, runs CTest and the offline checks: unit tests, generated sources, API contract and roadmap. `--deploy` copies `WSOCK32.dll` and `MT2Framework.dll` into the game folder; `--smoke` also starts the game through Steam, waits for `MT2 Mod Framework <version> loaded` in the profile's game log and closes the game. The game folder is `--game`, else `MT2_GAME_DIR`, else the Steam default. CMake comes from PATH or the Visual Studio installation. The build uses the Visual Studio generator with two jobs unless `CMAKE_GENERATOR` and `CMAKE_BUILD_PARALLEL_LEVEL` are set; CI sets `Ninja Multi-Config` with `sccache` as compiler launcher inside the Visual Studio developer shell.

Builds and detailed logs stay under ignored `build/` and `artifacts/`. `--core-only` omits the Windows adapter. `--skip-tests` skips CTest.

## Build documentation

```powershell
npm ci --ignore-scripts
python tools/generate_docs.py
npm run docs:build
npm run docs:test
```

The API contract generates the Lua SDK, public function/event/type pages, schemas and change report. The native ABI page includes the authoritative header declarations. Examples are included from real project files. `--check` reports stale outputs without rewriting them.

The documentation build produces `dist/docs/site/`, `dist/docs/offline/` and `dist/docs/markdown/`. Open the offline `index.html` directly. All images, search data and code copying work without a server or CDN. Browser tests use Playwright Chromium; set `CHROMIUM_EXECUTABLE` to reuse an installed browser.

## Build portable tools and a local product

```powershell
python tools/build_helper.py --bootstrap
build/helper-env/Scripts/python.exe tools/package_product.py
```

Build documentation first. The helper embeds the same generated SDK, contract, agent guide and offline documentation. The package builder includes only owned runtime files, SDK/templates, examples and notices. It never installs into the real game or publishes the result.

## Source map

`src/core` owns plugin/runtime services. `src/loader` owns bootstrap and forwarding. `adapters/mt2` owns game layouts and exact-build guards; its internal extracted namespace is not public SDK. `third_party` has pinned native sources and original notices. `licenses` records origin and file hashes.

Do not distribute original game binaries, archives, private profiles or disassembly. Preserve the original multiplayer project's license and data. Real-game acceptance requires an owned process and separate profile; evidence from documentation checks must not be described as game acceptance.
