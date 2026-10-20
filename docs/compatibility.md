---
title: Compatibility and status
---

# Compatibility and status

This is a public development preview: framework **0.2.3**, Lua API **1.0.0**, native ABI **1**. It supports only Windows x64 with Steam and the exact verified MT2 executable:

```text
bbab2dc68babcc644d0a1679548bf1e334b6c52aea669c3f61f6cb9d45c1aaf6
```

Unsupported game builds leave the adapter and mods disabled and report the cause. A signature match alone never establishes compatibility. The installer preserves unknown loaders and original game archives.

The runtime does not resolve internal game functions through the executable's symbol table. The adapter uses addresses and byte checks for the verified build. Symbols have helped with reverse engineering, so removing them makes future adapter work harder. A changed executable, including one with only its symbol table removed, still needs compatibility checks and an adapter update before support is enabled.

## Accepted behavior

Retained private-game checks cover independent Lua/native plugins, shared hooks, transactional Lua replacement, callback failure isolation, camera/time controls, native place/move/remove operations, stale handles, two authored GLB packs, persistent building identity and missing-content load rejection. Portable installation, update, ZIP import and removal were checked on a separate game copy, including Unicode profile/world names.

The empty-core p95 measured 0.3478 ms over 2048 samples in the retained paused fixture. The baseline includes the measurement probe and diagnostics, so this is not a pure uninstrumented-game benchmark. Candidate hashes remain in the acceptance record. New source/build checks do not retroactively change those historical hashes.

## Current limits

- Lua is primary. C# integration is not part of this candidate.
- GLB conversion is static scenery; animation and skeletons are unsupported.
- Selected objects expose verified public handle/position information, not arbitrary native properties.
- Root-world building references can persist; unsupported targets are rejected.
- Native plugins are not unloaded during a session. Native memory faults can terminate the game.
- Human newcomer timing is pending. Automated onboarding does not establish a human ten-minute claim.
- Multiplayer bridge F with two separate Steam accounts is pending. Shared writes and multiplayer compatibility are not advertised.

## Experimental patches in 0.2.0

The [patch API](experimental/patches.md) shares ordering between Lua and native plugins. It observes the existing native Update and object-create envelopes. `api.time.set` mediates Framework requests only; it does not intercept native game clock controls.

An isolated game run confirmed real Update pairs, a shared Lua/native create observation with a confirmed placed object, a Framework speed request limited from 4 to 2, rejected and successful Lua replacement, owner-only callback failure and world-epoch change. Exact binary identities and scope are recorded in [patch acceptance](public/evidence/patches-acceptance.json). Session-provider roles and retirement boundaries also have controlled native fixture coverage; this does not establish two-account multiplayer compatibility.

`mt2.time.set` now requires acknowledged standalone or trusted Host authority. An active supported Multiplayer plugin without a session provider has unknown authority and cannot use this operation. Clients are denied. The general multiplayer bridge fence still applies, including for a provider reporting Host.

## Reload and migration

Lua script reload stages a replacement and commits registrations at frame end. Failed replacements keep the old instance. Already executed world actions remain. Content and native plugin updates require restart.

Stable API changes stay compatible within the documented API major version. Check [generated changes](reference/changes.md), the versioned contract and migration notes before updating. A function signature check cannot establish unchanged behavior by itself.

## Modded-session identification

From 0.2.3, the Framework identifies itself and its version in the original game log before loading mods. Each loading mod is listed by ID, version and type, followed by its activation status. These startup messages help identify external code in bug and crash reports. They do not upload data or log player input, save contents or account identifiers. The exact supported game-build check remains in place.
