---
title: Native MT2 services
---

# Native MT2 services

Framework 0.1.5 provides experimental MT2 native services alongside stable plugin ABI 1. The declarations are in `adapters/mt2/sdk/native_services.h`, and the developer package includes them under `sdk/native/mt2`. Keep these versioned game services separate from the stable Lua and native contracts.

## Bootstrap and ownership

Use the regular `MT2FW_PluginQuery`, `MT2FW_PluginLoad` and `MT2FW_PluginShutdown` exports. Keep the exact borrowed host pointer received at Load. Load is preparation; discover the service in the first active callback by resolving `MT2FW_MT2_QueryService` on the already loaded `MT2Framework.dll`. Pass service version 1 and the full output size. An absent export or unavailable result means the required experimental service is unavailable. It never authorizes another loader.

The framework validates the actual host identity and supplies trusted game, profile, isolated game-data and mod-owned data paths. `game_data_directory` is the trusted profile container, not necessarily the active write root: PHYSFS may use its Steam-account child. Resolve the acknowledged write directory rather than assuming both paths are identical. All service registration, hook and capture-control calls belong in framework callbacks. Scope emission, capture draining and section lookup can run on workers. Workers must stop before plugin shutdown returns. A retired interface rejects new registrations; keep all native code and detour storage alive for the session. These services do not isolate native memory faults.

## Paired update callbacks

`frame_subscribe` registers both callbacks as one owned pair. A begin result of 1 enters the pair; 0 skips it. Each entered begin receives one end, in reverse subscription order. The frame contains the game receiver, a monotonic frame ID and delta time. End indicates whether original Update returned normally. The framework invokes original Update once between these phases. Existing ordinary `update` events remain after this pair.

Use the pair to preserve a simulation fence that must surround original Update. The after callback can retain the existing Multiplayer order: replay, transport, time, HUD, diagnostics, close the fence, then lobby preparation. Do not move a fence into the ordinary post-update event. A plugin retired during the original call still receives its entered end. Its native shutdown waits for this callback lease to close. Shutdown requested during a frame is deferred until the pair completes.

Callbacks must return promptly and must not throw across the native boundary. A throwing begin violates the contract and cannot promise cleanup of resources it acquired before throwing. Native teardown must not destroy detour state while an engine invocation is still using it.

## Hook catalog

The framework owns MinHook initialization, named target ownership, enable/disable and rollback. A native plugin can request a named adapter target; it cannot supply a target RVA or executable address. Different names cannot alias the same catalogued address. A claimed target cannot be claimed by another plugin. Failed installation and failed rollback retain ownership until actual cleanup succeeds.

The current candidate reserves existing framework targets, including original Update, title/game commands, native desktop and StartMenu, create/move/remove observers, SDL and PhysFS. It deliberately has no live unique Multiplayer catalog entries yet. The Multiplayer hook inventory must provide exact verified targets and installation phases first. Shared operations require typed adapter dispatch and an owned original-call receipt; they cannot use a second detour or blind chaining. This remains an unfinished migration slice.

The loader continues to reject the legacy `MT2_Multiplayer.dll`. The migrated plugin uses `MT2MultiplayerPlugin.dll`. It must not call the standalone bootstrap, install another profile redirect, initialize another renderer/input system, call `MH_ALL_HOOKS` or uninitialize the process hook manager.

## Optional profiling feed

Register immutable named sections once. Cache the service and section IDs. `scope_begin` returns `{epoch, token}`; always pass the same section/token to `scope_end`. With no capture active, begin returns zero before reading a clock or allocating memory. No per-scope JSON, file access or observer callback runs.

Only one plugin owns a capture at a time. `capture_start` preallocates 64 to 1,048,576 fixed 56-byte event slots. Producers use a bounded try-lock write and count overflow or contention as dropped events. The capture owner drains batches with `capture_read`, then stops and drains remaining records. Stop preserves the queue. Starting another capture explicitly replaces the previous queue and advances its epoch. Old scope tokens cannot close scopes in another epoch.

Events contain capture epoch, sequence, monotonic nanoseconds, thread ID, token, game-update frame ID, section ID and kind. Frame markers surround original Update and native pairs; they do not represent GPU completion, rendering or the full ordinary update callback period. Section metadata supplies the mod ID and name.

`dropped` is cumulative for the capture. Buffer overflow, writer contention and producer retirement can make an interval incomplete. The consumer must retain this state. It must not invent exact exclusive times, complete call trees or maxima for an incomplete interval. The profiler mod owns trees, aggregation, interval analysis, reports, comparisons, UI and exports. The framework contains only the transport buffer and markers. This feed reports elapsed wall time. `thread_cpu_available` is 0; coarse CPU sampling in a separate profiler must be labelled separately.

## Acceptance

Automated checks cover ownership, frame ordering and retirement, capture identity, concurrent producers, overflow, epochs, hook rollback and isolation between plugins. Actual game captures confirm balanced native sections and acknowledged export. A bounded paused-world Multiplayer capture confirms instrumented scopes; it does not establish rendered FPS, native thread CPU, active simulation performance or public Multiplayer compatibility.
