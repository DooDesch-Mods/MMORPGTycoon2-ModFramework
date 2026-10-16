---
title: Compatibility and status
---

# Compatibility and status

This is a local development candidate: framework **0.1.0**, Lua API **1.0.0**, native ABI **1**. It supports only Windows x64 with Steam and the exact verified MT2 executable:

```text
bbab2dc68babcc644d0a1679548bf1e334b6c52aea669c3f61f6cb9d45c1aaf6
```

Unsupported game builds leave the adapter and mods disabled and report the cause. A signature match alone never establishes compatibility. The installer preserves unknown loaders and original game archives.

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

## Reload and migration

Lua script reload stages a replacement and commits registrations at frame end. Failed replacements keep the old instance. Already executed world actions remain. Content and native plugin updates require restart.

Stable API changes stay compatible within the documented API major version. Check [generated changes](reference/changes.md), the versioned contract and migration notes before updating. A function signature check cannot establish unchanged behavior by itself.
