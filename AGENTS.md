# Framework contribution guide for agents

Read `README.md`, `CONTRIBUTING.md` and `docs/agents/start.md`. For a mod rather than a framework change, use the generated project's own AGENTS.md and bundled API/SDK.

- This source version targets Framework 0.3.1 and Lua API 1.2.0. C ABI remains 1. Match the installed release and keep these versions distinct from the supported game executable hash.
- Public Lua declarations and metadata originate in `api/contract.json`. Run `python tools/generate_docs.py`; do not hand-edit `sdk/mt2.lua`, `api/generated` or generated reference pages.
- Native ABI declarations originate in `include/mt2fw/plugin.h`. Native game layouts and hooks belong to `adapters/mt2`, outside the stable public API.
- Experimental gameplay operations must have an implemented adapter handler, argument metadata and a native result contract. Preserve exact UID strings and action outcomes. Never turn an accepted call or a successful build into a claim about game effects.
- Gameplay modules own `operations.json` and `types.json` beside their native source. After wiring the module into the adapter build and dispatcher, run `python tools/merge_gameplay.py` to update the canonical contract, then run the documentation generator. The merge rejects descriptors whose source or dispatch header is not registered.
- Runtime changes: run `python tools/build.py` and the affected tests. API/example changes: run `python tools/generate_docs.py --check` and `python tools/check_contract.py` after building the probe. Docs/theme changes: run `npm run docs:build` and affected browser views. See CONTRIBUTING for exact scope.
- Examples are complete projects under `examples/`; reference snippets are included from their real files. The docs probe runs real Lua with a synthetic host. It does not prove native game effects.
- Preserve original causes and never replay an unknown or timed-out mutation without checking current state. Never use a user's existing profile as a disposable test fixture.
- New captures require an owned game process/profile and explicit capture metadata. Do not ship original game files, private saves, disassembly, keys or machine-specific evidence paths.
- Work locally unless the human authorizes external actions. Preparing this repository does not authorize commits, pushes, publication, releases or hosting.
- Documentation and development-facing text are English. Keep the original Astra identity, readable contrast, responsive views and the exact support line required near public README titles.
