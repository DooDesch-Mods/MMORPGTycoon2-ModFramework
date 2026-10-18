---
title: Ordered patches
---

# Ordered patches

Use Lua to observe named native calls and apply a typed policy to Framework time requests. Requires **MT2 Mod Framework 0.2.0 or later** with **experimental patch protocol 1**, separate from stable Lua API **1.0.0** and native ABI **1**. Use the SDK that matches the runtime. The [machine contract](../public/api/experimental/patches.json) describes each target, phase, payload and permitted effect.

`game.update` and `objects.create` surround real native calls. `api.time.set` surrounds only requests made through `mt2.time.set`. It does **not** intercept the game's clock controls or every native clock call. All three targets use the framework's existing hook or request path. Lua mods need no raw addresses, signatures, compiler or native DLL.

## Observe a native create

Start with the complete SDK project `examples/patch-observer`. Its in-game window counts native create entries and returns after `world_ready`. Counts stay in memory; the example does not write a log on each frame or create.

1. Copy `examples/patch-observer` from the SDK to `C:/Mods/patch-observer`.
2. Run `mt2fw check C:/Mods/patch-observer`.
3. Run `mt2fw pack C:/Mods/patch-observer C:/Mods/patch-observer.zip`.
4. Install the ZIP with the portable helper, open the game and load a world.
5. Place an object and read the **Native create observer** window.

<<< ../../examples/patch-observer/main.lua{lua}

The complete project includes `mod.json`, editor definitions and its license. Its capabilities are `ui` and `world.read`; it is a local mod. A native create is not the same as a successful queued placement action. `original_returned` means the native call returned normally. `result_available` means the native result was non-null. It supplies no pointer or UID and does not replace the action API's `confirmed` result.

Lua observes create calls on the bound game thread. Worker-thread calls are not queued to a later frame and presented as surrounding the original. Native plugins retain their typed callback contract. Payloads are copied, and no raw pointer is exposed to Lua. Invocation, generation, thread and epoch IDs are exact decimal strings.

## Register and discover

Call `mt2.patches.add(spec, {before=..., after=...})` once during preparation. Supply at least one callback. `id` is unique within your mod; `target` is an exact catalog ID. `mode` defaults to `observe`, and priority defaults to zero. Observer callbacks return no value. Editing their copied table cannot modify the original call.

`mt2.patches.targets()` lists supported targets. `mt2.patches.status()` exposes the published generation, order and unresolved references. Both are available during preparation. Remove your own registration with `mt2.patches.remove(id)`.

`remove` returns true after the owner has no live patch-invocation leases. False means removal applies to future calls while a current pair still retains its callback references. Status includes each patch's latest error and a suppressed-error count.

| Target | Source | Effects | Capability |
| --- | --- | --- | --- |
| `game.update` | Native Update | Observe Before/After | `world.read` |
| `objects.create` | Native creation | Observe Before/After | `world.read` |
| `api.time.set` | Framework time request | Observe or typed paused/speed policy | `world.read` for observe; `time.write` for modify |

Patch callbacks must not issue world writes. Modifiers return proposed typed values instead. Use ordinary events and user-triggered UI for world actions, with their declared capabilities and authority checks. These patch targets do not provide argument writes for native calls, return replacement, skip-original, arbitrary detours or transpilers.

## Two observers can coexist

Copy the observer project to a second directory, change its manifest ID to `yourname.second`, and install both packages. Their IDs are independent. Two observers on `objects.create` are valid; sharing a target is not itself a conflict.

An explicit `before` or `after` list contains fully qualified `manifest-id:patch-id` strings. For example, a second patch can specify `after = {"example.patch-observer:count"}`. This refers to a patch, not a dependency or physical hook address. Absent references remain dormant and visible. A required mod also belongs in `mod.json` dependencies.

Explicit constraints take precedence. Ready patches use descending priority, then byte-ordinal manifest ID and patch ID. Before runs in that order; entered After callbacks run in reverse:

```text
example.patch-observer:count Before
yourname.second:count Before
one original native call
yourname.second:count After
example.patch-observer:count After
```

Lua and native registrations through this patch service share this order. Existing native services retain their established outer envelopes; a constraint does not reorder another service's legacy subscriptions. Registration changes affect a later invocation, not a half-completed pair.

## Apply a Framework time policy

The complete `examples/time-policy` project owns the typed `paused`/`speed` writer slot for `api.time.set`. Its Before callback returns a partial replacement, `speed`, clamped to 2. It leaves `paused` unchanged. Effective speed must remain finite and nonnegative; zero is allowed.

<<< ../../examples/time-policy/main.lua{lua}

Use the same check, pack and install steps with `time-policy` in place of `patch-observer`. Load a world, then press **Request speed 4 (limit 2)**. With acknowledged standalone authority, the returned clock shows speed 2. The visible result preserves a rejected authority cause. Requests happen only on a button click and are never retried automatically.

This policy applies to Framework requests from mods. The game's native clock controls are outside its scope. A second modify patch on `api.time.set` is an actual conflict because the combined paused/speed slot has one writer. The framework rejects the candidate instead of silently choosing the last writer. Read-only observers may still coexist.

## Authority and failures

A manifest declaration is not host authority. This version permits patch mutations only in a proven standalone session. Unknown sessions and clients cannot execute them. A trusted provider can acknowledge Host, but requests remain blocked because this version has no shared Host-action bridge. This feature makes no multiplayer compatibility claim.

An inactive but still loaded, pinned or retained Multiplayer bridge also has unknown authority. Disabling its mod does not prove that its session has disconnected. The original rejection reason stays visible.

Read the original error in Mods or Developer tools. Errors identify the owner, patch, target and failed condition. Unsupported native mutation, unknown target, invalid phase or type, an ordering cycle and an exclusive writer conflict reject the candidate. A cycle includes its closed path. A Lua callback error disables its owning mod and preserves the original cause. Other owners continue; observer failures cannot suppress or replay the original native call.

| Error | Meaning |
| --- | --- |
| `PATCH_TARGET: unknown target objects.creat` | Choose an exact ID from target discovery. |
| `PATCH_READ_ONLY: objects.create supports observe only` | This native target has no writable arguments. |
| `PATCH_READ_ONLY: observe callback must return nil` | Remove the observer's return value. |
| `PATCH_READ_ONLY: patch callbacks cannot call shared-world write APIs; modifiers return writable values` | Return the modifier's proposal or move the world request to an ordinary event or user action. |
| `PATCH_VALUES: speed must be finite and nonnegative` | Return a supported typed speed value. |
| `PATCH_WRITER_CONFLICT: api.time.set paused/speed owned by example.time-policy:limit-speed; rejected yourname.second:limit-speed` | Keep one writer; observers may coexist. |
| `PATCH_AUTHORITY_UNKNOWN: no acknowledged session authority` | The request was blocked before execution; no authority provider has acknowledged the session. |
| `Trusted Multiplayer provider reports Client; patch mutation denied` | This process is a client; it cannot execute the mutation. |
| `Trusted Multiplayer provider reports unknown session authority` | The provider has not established a usable role. |
| `Multiplayer bridge is active without an acknowledged session provider` | A bridge is present, but it has supplied no current authority acknowledgement. |
| `Trusted Multiplayer provider acknowledged Host; shared Host-action bridge is unavailable, patch mutation denied` | The role is known, but this version cannot route the request through a shared Host action. |

The session adapter can provide a more specific client, unknown-session or transition reason. Preserve that text. If a writer throws or returns invalid values, the request reports `PATCH_VALUES: api.time.set writer failed; original request was not executed`; the attributed callback log retains the original cause.

An initially denied time request enters no patch callback. After Before has run, a rejected writer or changed authority reports `original_returned=false, outcome="rejected"` to the other entered After callbacks. A confirmed adapter return supplies `outcome="confirmed"` and its clock `result`; an adapter exception supplies `outcome="unknown"`. A failed Before does not enter its own After.

## Use the native C++ SDK

Start from the complete `examples/native-patches` project. Its manifest declares `world.read`, and its CMake project includes the matching bundled native headers. The plugin registers one typed create observer from an active update callback. It resolves the extension from the already loaded framework and passes the exact borrowed Host pointer received at Load. No alternative loader or game address is needed.

<<< ../../examples/native-patches/plugin.cpp{cpp}

After the first completed create, the next active update requests retirement. `BUSY` stops future selection but requires callback code and user state to stay valid until a later retirement acknowledgement is `OK`. A failure preserves state and stops automatic retries. This example keeps its state for the process lifetime, so failure also cannot free live callback storage. Receipts and first retirement-state changes go to the Windows debugger output. Framework-owned retirement runs before plugin shutdown; never unload your DLL or free callback state to force a `BUSY` result to disappear. Callback records are borrowed until return, and no C++ exception may cross the C boundary.

## Reload safely

Lua reload stages the owner's entire replacement set. If preparation, typing, authority-independent registration validation or ordering fails, the previous Lua instance and published order stay active. **Active** and **Reload rejected** describe separate states. Fix the reported cause, then reload again. A successful reload replaces the old registrations and resets example counters. Native plugins require restart.

Reload does not undo a confirmed world change. Preserve an unknown world-action outcome and inspect current state; never replay it automatically.
