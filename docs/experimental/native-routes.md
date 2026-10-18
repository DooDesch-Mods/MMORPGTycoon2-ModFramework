---
title: Native shared routes and hook ownership
---

# Native shared routes and hook ownership

Framework 0.1.5 provides experimental shared routes through `MT2FW_MT2_QueryRoutes`, version 1, with the exact host pointer from PluginLoad. Declarations are in `adapters/mt2/sdk/native_routes.h` and the developer package's `sdk/native/mt2` directory. Stable plugin ABI 1 remains unchanged.

## Named catalog and phased hooks

`adapters/mt2/multiplayer-catalog.json` maps 131 unique EXE targets to stable private names, source groups and installation phases. It retains the sealed Multiplayer inventory and adds the verified Monster Release capture route. Eleven existing EXE hooks remain Framework-owned. Catalog admission does not install or enable any Multiplayer hook. Existing initial, world-ready and lazy owner guards still apply. Dormant pattern scanners, debug-register probes and exception traps are excluded.

1. Query the route service in an active Framework callback.
2. Call `hook_prepare` with a catalog name and your detour. Require `OK`, `PREPARED`, the expected owner/entry and a nonzero actual trampoline.
3. Publish your existing disabled receiver with that trampoline, retaining your current body, thread and owner checks.
4. Call `hook_enable`. Require `OK` and `ENABLED`, then validate the owned route receipt.
5. Retire only after the feature's own quiescence and reclamation guards permit it. Use `hook_remove` for the exact owner and named target.

The Framework owns MinHook, individual target activation and rollback. Plugins never initialize or uninitialize it and never use enable-all or disable-all. A known failed `MH_CreateHook` does not acquire another owner's slot. An unknown operation or changed live route cannot be automatically replayed or blindly overwritten. Receipts retain the failed condition and actual state. DLLs, originals and native detour storage remain session-owned; there is no general hot-unload claim.

Each receipt includes executable hash, RVA, entry, trampoline, detour, generation, owner, acknowledged pristine/current prefixes, state and original cause. The Windows backend reads the exact executable once, requires its supported SHA256, maps executable PE sections and checks live bytes. Enabled paths must also resolve through a bounded jump proof to the acknowledged detour. A receipt is a current observation, not permission to ignore later changes.

## Original code admission

Keep existing native guard expectations. For a patched named target, call `code_verify` with its target-relative offset and the original expected bytes. This checks the expected bytes against the verified executable, the acknowledged current patch and unchanged live body bytes outside the patch prefix. Windows, DLL and address values stay in the private adapter contract.

In particular, Move at `0x55ed80` has no Multiplayer detour but is observed by the Framework. A successful `objects.move` receipt plus original-body guard admission replaces the assumption that its entry must be unpatched. Native Move calls still use the receipt's entry and traverse the observer exactly once. Do not call its trampoline to bypass observation. The same receipt mechanism covers Framework Update, title/game/save commands and desktop/StartMenu routes that the older resolver checked as pristine.

## CreateBuilding

`create_subscribe` supplies typed before/after observations of one unchanged original call. It permits no suppression or argument rewrite. The after callback receives the exact returned pointer and whether original returned normally. The adapter retains and rethrows original C++ exceptions after the after observations. Each invocation has its own ID, so nested calls do not share mutable per-call identity.

The underlying Framework prop observer stays inside the original continuation. Multiplayer retains its own scoped attempted/returned/result observation. Purchase and update suppression stays in the unique upstream routes. Callback errors are logged on the game thread without inventing an original result or executing another original.

## DestroyProps authority

`remove_bind` registers one authority owner. The adapter's noinline native entry captures the original return-slot address, immediate return address and thread before calling the dispatcher. The borrowed slot still refers to the native caller's frame; it is not a frame inside the added dispatcher. The Multiplayer adapter can reuse its exact caller-frame classification and existing capture/replay policy.

The authority receives a typed per-invocation continuation. It retains the original manager/array and rejects a second attempt. Return `SUPPRESSED` only when no original was attempted, `FORWARDED` only after one normal original return, or `UNKNOWN` when the result is uncertain. Unknown, callback failure and inconsistent dispositions never trigger an automatic original. The original C++ exception is retained internally, reported as a failed continuation, then rethrown after the authority returns.

Workers bypass game-thread authority and forward unchanged once. Unowned routes also forward once. On the game thread, foreign/standalone/empty/authorized-replay decisions remain in the existing Multiplayer authority code. Owned UI rejection remains suppressed. The Framework does not recreate that policy from the number of selected objects.

Suspend the Multiplayer authority/session before unbinding. A busy unsubscribe/retirement means an invocation still owns the callback/user data. The owner must retain that state until confirmed quiescence. Framework shutdown keeps the runtime if a hook or shared invocation cannot retire. It does not report cleanup success while a route remains live.

## Acceptance and limits

The separate [ownership extension](native-ownership.md) supplies typed `msvcrt!free` import control, retained disable/resume and passive Mode/input observations. ServicesV1 and RoutesV1 remain separately versioned contracts. Plugins must preserve their native free/delete/reclamation policy when adopting shared routes. Retained game tests cover the migrated place/move/remove action cycle; they do not establish full Multiplayer compatibility or support for arbitrary native hooks.

Local tests exercise Create original-once/result/exception, Remove suppression/continuation/provenance/worker/failure paths, named ownership, prepare/enable ordering, guard admission, foreign patch rejection, failed creation isolation and unknown outcomes. They use synthetic memory and functions. The Windows build confirms compilation; it does not prove caller provenance or gameplay effects in the actual game.
