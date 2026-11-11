# MT2 Mod Framework

Lua and C++ mod framework for MMORPG Tycoon 2 (Windows x64). `api/contract.json` names the Framework version (`framework_version`), the Lua API (`api_version`) and the native C ABI (`abi_version`). CMake, the DLL version resource, the docs and the README roadmap read them from there. Keep these distinct from the supported game executable hash. API 1.3 functions need Framework 0.4.0 or later.

Read `README.md`, `CONTRIBUTING.md` and `docs/contributing/build.md` first. `docs/agents/start.md` is for agents that write a mod, not for changes to the framework.

## Layout

- `src/core`: plugin and runtime services. `src/loader`: bootstrap and forwarding.
- `adapters/mt2`: game layouts, hooks and exact-build guards. Not public API.
- `include/mt2fw/plugin.h`: native C ABI, the only source for it.
- `api/contract.json`: source of the Lua API. `api/experimental/`: gameplay and patch contracts. `api/baselines/`: accepted API releases.
- `sdk/`, `templates/`, `examples/`: shipped to modders. `examples/` are complete projects; docs snippets are included from their real files.
- `tools/`: build, docs, packaging and release scripts. `tests/`: unit tests. `integrations/`: Nexus.
- `docs/`: VitePress site. `third_party/`: pinned native sources.

## Commands

Needs Visual Studio 2022 C++, Python 3.11+ (`pip install -r requirements-dev.txt`) and Node 22+. `build.py` takes CMake from PATH or from the Visual Studio installation.

```powershell
python tools/build.py --check    # native build, CTest, unit tests, generated sources, contract, roadmap
python tools/build.py --deploy   # build, copy WSOCK32.dll and MT2Framework.dll into the game
python tools/build.py --smoke    # deploy, start the game through Steam, wait for the load line, close the game
npm ci --ignore-scripts; npm run docs:build; python tools/check_docs.py; npm run docs:test
```

The game folder is `--game`, else `MT2_GAME_DIR`, else the Steam default. `--deploy` refuses a running game and a foreign `WSOCK32.dll`, and writes `mt2fw.json` with `<game>/mt2fw-profile` only when none exists. `--smoke` uses the profile in `mt2fw.json`. `checks.yml` runs `build.py --check` on every change outside `docs/`, `assets/` and root Markdown files; `docs.yml` builds and tests the docs when docs inputs change.

## Rules for a change

- Do not hand-edit `sdk/mt2.lua`, `api/generated/`, `src/core/gameplay_lua.inc` or `adapters/mt2/gameplay/operations.inc`. Change `api/contract.json`, run `python tools/generate_docs.py` and commit the result.
- `docs/reference/`, `docs/public/api/1.3.0`, `docs/public/api/experimental`, `search-index.js`, `llms.txt` and the dependency notices are ignored. `npm run docs:build` writes them.
- Gameplay modules own `operations.json` and `types.json` next to their native source. After wiring the module into the adapter build and dispatcher, run `python tools/merge_gameplay.py`, then the docs generator.
- An experimental gameplay operation needs an implemented adapter handler, argument metadata and a native result contract. Keep UIDs as exact strings.
- Never replay an unknown or timed-out mutation without checking the current state. Never claim game effects from an accepted call or a green build. A synthetic-host test proves API wiring only.
- Never update the 1.0.0 baseline to hide a breaking change. Add a new versioned baseline for an accepted API release.
- Some scripts write tracked files (`docs/media.json`, license inventories). Check `git status` after a run and do not commit accidental writes.
- Real-game checks need an owned game process and profile, never a user's existing profile. Ship no original game files, private saves, disassembly, keys or machine-specific evidence paths.
- `templates/AGENTS.md` is a product file for generated mods. Keep its API version current.
- Docs and development text are English. Public README titles keep the support line.
- Work locally unless the human authorizes commits, pushes, releases or hosting.
