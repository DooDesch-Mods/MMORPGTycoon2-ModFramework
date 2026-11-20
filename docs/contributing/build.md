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

`--live` deploys into an owned copy of the game folder, starts `MT2.exe` from it, creates a new world named `MPTestLive<timestamp>` and runs `network.catalogue`, `network.snapshot`, `objects.list`, one native remove and a screenshot capture through `<profile>/diagnostics/request.json`, then exits the game. Every step must pass, and each prints one `info: live ...` line. It refuses a folder under `steamapps`: copy the game folder elsewhere first, so worlds and removals land in the copy's own `mt2fw.json` profile.

```powershell
python tools/build.py --live --game <owned game copy>
```

## Port to a new game build

A Steam update changes `MT2.exe`, and the adapter's addresses, pins and guards then refer to the old build. `tools/port_game_build.py` moves them. It needs `pip install pefile capstone` and both executables; keep a copy of the old build before Steam replaces it.

```powershell
python tools/port_game_build.py <old MT2.exe> <new MT2.exe>
python tools/port_game_build.py <old MT2.exe> <new MT2.exe> --accept tools/ports/<old>-to-<new>.accept.txt --audit
python tools/port_game_build.py <old MT2.exe> <new MT2.exe> --accept tools/ports/<old>-to-<new>.accept.txt --apply
python tools/build.py --check
python tools/build.py --live --game <owned copy of the new build>
```

1. The first run writes `port-report.tsv`: every pin, guard and address it could not prove. Read the instruction diff of each changed function. If the change does not touch what the adapter relies on, list it in `tools/ports/<old>-to-<new>.accept.txt` as `OLD[=NEW] reason`; `=NEW` pairs a function the map did not match.
2. `--audit` checks the function map without rewriting. For every function start the adapter or tests name, and every accept entry, the new function must read the mapped `.data`/`.bss` globals of the old one. Byte similarity alone can pick the wrong template twin. Each mismatch prints the expected and the actual globals and the candidates: functions of the same size within 0x40000 that read the expected globals and make the same direct calls. Pair a candidate in the accept list and run `--audit` again. Run it before `--apply`, while the sources still hold the old addresses.
3. `--apply` rewrites the sources. Then build, check and run `--live` on an owned copy of the new build.

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
