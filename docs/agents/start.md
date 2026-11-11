---
title: Build a mod with an agent
---

# Build a mod with an agent

Use the same public contract and complete examples as a human modder. The current Lua API is **1.3.0**; the native C ABI is **1**. Set `api_version` in `mod.json` to the oldest Lua API that has every function you call. The [API reference](../reference/index.md) lists `Since` and `Requires Framework` per function; older runtimes refuse a newer `api_version`. The [compatibility page](../compatibility.md) names the supported game build.

## Context to give your agent

- This page, the [quickstart](../quickstart.md), the [API reference](../reference/index.md) and the [machine contract](../public/api/1.3.0/api.json).
- `mt2fw new` adds `AGENTS.md`, `.mt2fw/mt2.lua` and `.mt2fw/api.json` to a new mod for offline use. Complete examples are under `examples/` in the SDK and the repository.
- `.md` counterparts of the HTML pages, or [llms.txt](../public/llms.txt), for compact reading. An agent does not discover them on its own; pass the entry file or link.
- Central Mods entries, native windows and world dashboards: the [Lua apps guide](../recipes/lua-apps.md). Queries and the paired fee action: the [world-query recipes](../recipes/world-queries.md).
- Dungeon, character, actor, Region and management operations: the [experimental gameplay guide](../experimental/gameplay.md) and its [contract](../public/api/experimental/gameplay.json). Query from game callbacks, cache for render, and check the action status before treating a mutation as done.
- Typed patches: the [patch guide](../experimental/patches.md) and [contract](../public/api/experimental/patches.json). Never infer Host authority from a manifest or a missing session flag.
- The native [Step SDK](../experimental/steps.md) and its [recovery extension](../experimental/step-recovery.md) belong to the native Multiplayer owner. They add no Lua functions or permissions.

## Author and validate

```powershell
mt2fw new yourname.hello C:/Mods/yourname.hello --template hello
mt2fw check C:/Mods/yourname.hello
mt2fw pack C:/Mods/yourname.hello C:/Mods/hello-1.0.0.zip
```

Edit only the generated mod. Keep its namespace and declare capabilities. Register windows once, keep widget calls inside render callbacks, and wait for `world_ready` before world-dependent work; `start` is still preparation. To add a setting, define it with `mt2.config.define` and read it inside your window (`examples/config`).

## Rules that prevent damaging mistakes

- Preserve exact handle UIDs as strings. World epochs expire. Persist supported building references with world storage and confirmed saves.
- Accepted actions are queued. Only confirmed is success. Preserve unknown outcomes and inspect state; never auto-retry a mutation.
- Lua reload replaces registrations but does not undo world changes. Content and native changes need a restart.
- Use public APIs only. Raw addresses, arbitrary process starts and native Lua modules are outside the Lua contract.
- A synthetic host proves syntax and API wiring, not native world effects. Keep source and package checks apart from real game checks.

## Verify the result

Run `check`, pack outside the source folder and keep the package hash. Load the mod in an owned profile, edit the greeting, reload and look at the result. Ask the human only for missing intent, not for routine reversible choices.

Framework contributors: see the repository `CLAUDE.md` and the [contributor build guide](../contributing/build.md). For a native mod that depends on a Runtime fix, use the [loaded Runtime version check](../compatibility.md#runtime-versions-for-native-mods).
