---
title: Build a mod with an agent
---

# Build a mod with an agent

Use the same public contract and complete examples as a human modder. The current Lua API is **1.0.0**; the framework is **0.2.1** and native C ABI is **1**. The [compatibility page](../compatibility.md) identifies the supported game build.

## Context to give your agent

Read this page, the [quickstart](../quickstart.md), [API reference](../reference/index.md) and [machine contract](../public/api/1.0.0/api.json). A generated project includes `AGENTS.md`, `.mt2fw/mt2.lua` and `.mt2fw/api.json` for offline use. Source examples are under `examples/` in the SDK documentation bundle and repository.

Use `.md` counterparts of the HTML pages for compact reading, or follow [llms.txt](../public/llms.txt). Discovery by an agent is not automatic; provide the entry file or link explicitly. Documentation exports include a versioned Markdown copy under `versions/1.0.0/`.

Experimental typed patches require Framework 0.2.0 or newer. Read the [patch guide](../experimental/patches.md) and [patch target contract](../public/api/experimental/patches.json) before using them. Never infer Host authority from a manifest or a missing session flag.

## Author and validate

```powershell
mt2fw new yourname.hello C:/Mods/yourname.hello --template hello
mt2fw check C:/Mods/yourname.hello
mt2fw pack C:/Mods/yourname.hello C:/Mods/hello-1.0.0.zip
```

Edit only the generated mod. Keep its namespace and declare capabilities. Register windows once, keep widget calls inside render callbacks, and wait for `world_ready` before world-dependent work. `start` is still preparation. The first Hello template has a `local message` line that can be changed directly.

To add a setting, define it with `mt2.config.define` and read it inside your window. Use the complete `examples/config` project and the [function contract](../reference/functions/config-define.md). Lua needs no compiler.

## Rules that prevent damaging mistakes

- Preserve exact handle UIDs as strings. World epochs expire. Persist supported building references with world storage and confirmed saves.
- Accepted actions are queued. Only confirmed is success. Preserve unknown outcomes and inspect state; never auto-retry a mutation.
- Lua reload replaces registrations but does not undo world changes. Content/native changes need restart.
- Use public APIs only. Raw addresses, arbitrary process starts and native Lua modules are outside the regular Lua contract.
- Keep source/package checks distinct from real game acceptance. A synthetic host can prove syntax and API wiring, not native world effects.

## Verify the result

Check the generated manifest, run `check`, pack outside the source folder, and retain output and package hash. Then load the mod in an owned profile, edit the greeting, reload and inspect the visible result. Ask the human for genuinely missing intent, not for routine reversible authoring choices.

Framework contributors should also read repository `AGENTS.md` and [contributor build guide](../contributing/build.md). Human newcomer timing and multiplayer bridge acceptance remain separate release gates.
