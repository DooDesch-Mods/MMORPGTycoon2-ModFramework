---
title: Mod and content manifests
---

# Mod and content manifests

Every package has one `mod.json` at its root.

```json
{
  "id": "yourname.hello",
  "name": "My first mod",
  "version": "1.0.0",
  "api_version": "1.0.0",
  "type": "lua",
  "entry": "main.lua",
  "capabilities": ["ui"],
  "multiplayer": "local"
}
```

Required keys are ID, name, SemVer version, API version, type, entry and capabilities. IDs are lowercase and namespaced. Types are `lua`, `native` or `content`. Entry paths are confined to the package. Optional `dependencies` maps mod IDs to supported exact, `^` or `>=` version requirements. Missing dependencies, duplicate IDs and cycles receive concrete errors.

The default multiplayer class is `singleplayer`. `local` is for UI, input and local camera, and cannot request world/content/time writes. `shared` requires matching packages and the future accepted multiplayer bridge; it does not enable shared writes in this candidate.

Set `api_version` to `1.1.0` for the new Lua app and world-query functions. Older runtimes reject that requirement; existing API 1.0 mods stay supported.

Machine schemas: [mod.schema.json](../public/api/1.1.0/mod.schema.json) and [content.schema.json](../public/api/1.1.0/content.schema.json). They describe structure. `mt2fw check` additionally checks SemVer rules, Windows-confined paths, existing resources and package limits. Custom schema formats are `semver`, `supported-mt2-api` and `mt2-relative-path`; generic validators must register them or use the CLI for semantic validation.

Content definitions require `id`, `variant`, `model` and `material`. Resource paths remain inside the package. Use the converter instead of editing binary files by hand. See [capabilities](capabilities.md) and [scenery](../guides/scenery.md).
