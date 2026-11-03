---
title: Compatibility and status
---

# Compatibility and status

Framework **0.5.6** provides Lua API **1.3.0** and native ABI **1**. It supports only Windows x64 with Steam and the exact verified MT2 executable:

```text
bbab2dc68babcc644d0a1679548bf1e334b6c52aea669c3f61f6cb9d45c1aaf6
```

Unsupported game builds leave the adapter and mods disabled and report the cause. A signature match alone never establishes compatibility. The installer preserves unknown loaders and original game archives.

The runtime does not resolve internal game functions through the executable's symbol table. The adapter uses addresses and byte checks for the verified build. Symbols have helped with reverse engineering, so removing them makes future adapter work harder. A changed executable, including one with only its symbol table removed, still needs compatibility checks and an adapter update before support is enabled.

## Runtime versions for native mods

From Framework 0.5.4, the loaded `MT2Framework.dll` includes standard Windows
`VERSIONINFO`. Both fixed FileVersion and ProductVersion are `0.5.6.0`; their
display strings are `0.5.6`. The resource type is `VFT_DLL`, and
`OriginalFilename` is `MT2Framework.dll`. String values use table `040904b0`
with English/Unicode translation `0409`/`1200`.

For a native mod that needs a specific Runtime fix, read this resource from the
actually loaded Framework module and compare the numeric version. Do not infer
the loaded version from a package manifest or another DLL on disk. A missing or
unreadable resource does not satisfy a Runtime minimum. Releases before 0.5.4
do not provide this resource.

The Runtime version, Lua API version, native ABI version and exact experimental
draft versions are separate requirements. This resource adds no Host fields or
new Lua functions.

## Accepted behavior

Retained private-game checks cover independent Lua/native plugins, shared hooks, transactional Lua replacement, callback failure isolation, camera/time controls, native place/move/remove operations, stale handles, two authored GLB packs, persistent building identity and missing-content load rejection. Portable installation, update, ZIP import and removal were checked on a separate game copy, including Unicode profile/world names.

The empty-core p95 measured 0.3478 ms over 2048 samples in the retained paused fixture. The baseline includes the measurement probe and diagnostics, so this is not a pure uninstrumented-game benchmark. Candidate hashes remain in the acceptance record. New source/build checks do not retroactively change those historical hashes.

## Current limits

- Lua is primary. C# integration is not part of this candidate.
- GLB conversion is static scenery; animation and skeletons are unsupported.
- Selected objects expose verified public handle/position information, not arbitrary native properties.
- Root-world building references can persist; unsupported targets are rejected.
- Native plugins are not unloaded during a session. Native memory faults can terminate the game.
- Multiplayer is an Early Beta. Use the matching Runtime and Multiplayer versions and check the [Multiplayer feature overview](https://github.com/DooDesch-Mods/MMORPGTycoon2-Multiplayer#what-works-in-co-op) for the supported co-op scenarios. General Lua gameplay writes remain singleplayer-only.

## Experimental patches in 0.2.0

The [patch API](experimental/patches.md) shares ordering between Lua and native plugins. It observes the existing native Update and object-create envelopes. `api.time.set` mediates Framework requests only; it does not intercept native game clock controls.

An isolated game run confirmed real Update pairs, a shared Lua/native create observation with a confirmed placed object, a Framework speed request limited from 4 to 2, rejected and successful Lua replacement, owner-only callback failure and world-epoch change. Exact binary identities and scope are recorded in [patch acceptance](public/evidence/patches-acceptance.json). Session-provider roles and retirement boundaries also have controlled native fixture coverage; this does not establish two-account multiplayer compatibility.

`mt2.time.set` now requires acknowledged standalone or trusted Host authority. An active supported Multiplayer plugin without a session provider has unknown authority and cannot use this operation. Clients are denied. The general multiplayer bridge fence still applies, including for a provider reporting Host.

## Reload and migration

Lua script reload stages a replacement and commits registrations at frame end. Failed replacements keep the old instance. Already executed world actions remain. Content and native plugin updates require restart.

Stable API changes stay compatible within the documented API major version. Check [generated changes](reference/changes.md), the versioned contract and migration notes before updating. A function signature check cannot establish unchanged behavior by itself.

## Modded-session identification

From 0.2.3, the Framework identifies itself and its version in the original game log before loading mods. Each loading mod is listed by ID, version and type, followed by its activation status. These startup messages help identify external code in bug and crash reports. They do not upload data or log player input, save contents or account identifiers. The exact supported game-build check remains in place.

## Multiplayer Early Beta 29

Multiplayer Early Beta 29 requires Framework Runtime **0.5.6** for character-card previews and shared class renaming. Both players need the matching Multiplayer and Runtime versions. The native preview selector is experimental Service V2; stable plugin ABI 1, Lua API 1.3.0 and its 254 functions stay unchanged. See the [native service contract](experimental/native-services.md) for exact ownership and failure behavior.
