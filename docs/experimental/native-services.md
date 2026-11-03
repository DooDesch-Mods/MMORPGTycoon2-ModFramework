---
title: Native MT2 services
---

# Native MT2 services

Experimental MT2 native services were introduced in Framework 0.1.5 alongside stable plugin ABI 1. The declarations are in `adapters/mt2/sdk/native_services.h`, and the developer package includes them under `sdk/native/mt2`. Keep these versioned game services separate from the stable Lua and native contracts.

## Bootstrap and ownership

Use the regular `MT2FW_PluginQuery`, `MT2FW_PluginLoad` and `MT2FW_PluginShutdown` exports. Keep the exact borrowed host pointer received at Load. Load is preparation; discover the service in the first active callback by resolving `MT2FW_MT2_QueryService` on the already loaded `MT2Framework.dll`. Pass service version 1 and the full output size. An absent export or unavailable result means the required experimental service is unavailable. It never authorizes another loader.

The framework validates the actual host identity and supplies trusted game, profile, isolated game-data and mod-owned data paths. `game_data_directory` is the trusted profile container, not necessarily the active write root: PHYSFS may use its Steam-account child. Resolve the acknowledged write directory rather than assuming both paths are identical. All service registration, hook and capture-control calls belong in framework callbacks. Scope emission, capture draining and section lookup can run on workers. Workers must stop before plugin shutdown returns. A retired interface rejects new registrations; keep all native code and detour storage alive for the session. These services do not isolate native memory faults.

## Paired update callbacks

`frame_subscribe` registers both callbacks as one owned pair. A begin result of 1 enters the pair; 0 skips it. Each entered begin receives one end, in reverse subscription order. The frame contains the game receiver, a monotonic frame ID and delta time. End indicates whether original Update returned normally. The framework invokes original Update once between these phases. Existing ordinary `update` events remain after this pair.

Use the pair to preserve a simulation fence that must surround original Update. The after callback can retain the existing Multiplayer order: replay, transport, time, HUD, diagnostics, close the fence, then lobby preparation. Do not move a fence into the ordinary post-update event. A plugin retired during the original call still receives its entered end. Its native shutdown waits for this callback lease to close. Shutdown requested during a frame is deferred until the pair completes.

Callbacks must return promptly and must not throw across the native boundary. A throwing begin violates the contract and cannot promise cleanup of resources it acquired before throwing. Native teardown must not destroy detour state while an engine invocation is still using it.

## Hook catalog

The framework owns MinHook initialization, named target ownership, enable/disable and rollback. A native plugin can request a named adapter target; it cannot supply a target RVA or executable address. Different names cannot alias the same catalogued address. A claimed target cannot be claimed by another plugin. Failed installation and failed rollback retain ownership until actual cleanup succeeds.

The adapter catalog reserves Framework targets, including original Update, title/game commands, native desktop and StartMenu, create/move/remove observers, SDL and PhysFS. It also supplies named, version-pinned Multiplayer targets with explicit installation phases. Catalog admission alone does not install a hook or confirm a gameplay feature. Use the [native shared routes](native-routes.md) service for phased preparation, activation and ownership receipts. Shared operations require typed adapter dispatch and an owned original-call receipt; they cannot use a second detour or blind chaining.

The loader continues to reject the legacy `MT2_Multiplayer.dll`. The migrated plugin uses `MT2MultiplayerPlugin.dll`. It must not call the standalone bootstrap, install another profile redirect, initialize another renderer/input system, call `MH_ALL_HOOKS` or uninitialize the process hook manager.

## Character preview selector (Framework 0.5.6)

`MT2FW_MT2_ServiceV2` adds a restricted Character preview selector for the migrated `doodesch.multiplayer` plugin. Query the existing export with `MT2FW_MT2_PREVIEW_SERVICE_VERSION` (2), `&service.v1` and `sizeof(service)`. Require `OK`, version 2, the full returned size and both selector function pointers. Service V1 and stable plugin ABI 1 keep their existing layouts. Framework 0.5.5 rejects this version; missing support does not authorize a separate RNG hook.

The Framework remains the sole physical owner of `GetInt`. Its existing source selection runs first. The selector applies only to the two catalogued Character preview callers, the bound game thread and the supported Default RNG receiver. Multiplayer owns the exact InfoView/AttachWeapon scope and an initialized private RNG source. This service is not a general RNG replacement or a Lua API.

Bind in an active game-thread callback and retain the returned owner token. Return `UNAVAILABLE` only when the current call is outside the owned preview scope. Return `OK` with the complete selection struct, a 40-byte initialized private source and a terminated `cause` buffer. The source must be aligned, committed writable storage and must not overlap the shared RNG or its pointer slot. The callback must not throw. The Framework calls the original once using the final selected source and traces that source.

Callback errors, exceptions, malformed `OK` output and invalid sources retain their original cause, fail the registration and raise `MT2FW_MT2_CHARACTER_PREVIEW_ABORT`. The Framework releases the callback lease before this abort. It neither calls the shared RNG as a fallback nor invents an integer result. The consumer must catch this fixed abort at its POD native boundary, retain an ingress failure and refuse connected Character originals until controlled teardown. Original game exceptions propagate unchanged.

The owner lease covers selection, the original call and trace completion, including native SEH unwinding. Unbind disables new selection immediately and returns `BUSY` while the current call still owns the source; retain callback state and source storage until quiescence, then complete unbinding. Plugin retirement uses the same native ownership and deferred shutdown rules. Do not unload or replay an uncertain native call.

## Optional profiling feed

Register immutable named sections once. Cache the service and section IDs. `scope_begin` returns `{epoch, token}`; always pass the same section/token to `scope_end`. With no capture active, begin returns zero before reading a clock or allocating memory. No per-scope JSON, file access or observer callback runs.

Only one plugin owns a capture at a time. `capture_start` preallocates 64 to 1,048,576 fixed 56-byte event slots. Producers use a bounded try-lock write and count overflow or contention as dropped events. The capture owner drains batches with `capture_read`, then stops and drains remaining records. Stop preserves the queue. Starting another capture explicitly replaces the previous queue and advances its epoch. Old scope tokens cannot close scopes in another epoch.

Events contain capture epoch, sequence, monotonic nanoseconds, thread ID, token, game-update frame ID, section ID and kind. Frame markers surround original Update and native pairs; they do not represent GPU completion, rendering or the full ordinary update callback period. Section metadata supplies the mod ID and name.

`dropped` is cumulative for the capture. Buffer overflow, writer contention and producer retirement can make an interval incomplete. The consumer must retain this state. It must not invent exact exclusive times, complete call trees or maxima for an incomplete interval. The profiler mod owns trees, aggregation, interval analysis, reports, comparisons, UI and exports. The framework contains only the transport buffer and markers. This feed reports elapsed wall time. `thread_cpu_available` is 0; coarse CPU sampling in a separate profiler must be labelled separately.

## Acceptance

Automated checks cover ownership, frame ordering and retirement, capture identity, concurrent producers, overflow, epochs, hook rollback and isolation between plugins. Actual game captures confirm balanced native sections and acknowledged export. A bounded paused-world Multiplayer capture confirms instrumented scopes; it does not establish rendered FPS, native thread CPU, active simulation performance or public Multiplayer compatibility.
