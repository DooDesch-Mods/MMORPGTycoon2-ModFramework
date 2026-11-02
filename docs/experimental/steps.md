---
title: Coordinate native multiplayer steps
---

# Coordinate native multiplayer steps

Framework **0.5.0** includes experimental **Step draft 9**. It gives the supported
Multiplayer plugin a shared boundary for simulation, saving and leaving a
session. Lua API **1.3.0** keeps its existing 254 functions, and stable native
plugin ABI **1** is unchanged.

This extension is restricted to the exact active `doodesch.multiplayer` native
plugin and its registered session provider. Other mods should use the public
[Lua API](../reference/index.md), [native plugin ABI](../reference/native.md) and
[patch extension](patches.md). Declaring a manifest capability does not grant
Step authority.

## Character-update routes in Framework 0.5.2

Framework 0.5.2 adds three exclusive native routes for the matching Multiplayer
plugin: Toon scheduling, travel rotation and attribute cadence. Their pinned
targets are listed in the [native catalog](https://github.com/DooDesch-Mods/MMORPGTycoon2-ModFramework/blob/main/adapters/mt2/multiplayer-catalog.json).
The Framework admits and verifies these routes; the Multiplayer plugin owns the
same-receiver scope, branch decisions and preservation of live registers, flags
and stack. Installing the Framework alone does not change those game decisions.

Native code checks recognize multiple authenticated hooks inside a named code
window. They still reject unknown routes, mismatched pristine code and foreign
changes. The Routes interfaces and their 4096-byte verification bound are
unchanged. Larger pristine function checks must run before their first hook;
subsequent checks must preserve exact seam, trampoline, ownership and lifetime
proof. A failed or unknown hook operation must not be replayed automatically.

These routes add no Lua functions or authority. Use the Runtime required by the
Multiplayer release; Lua API 1.3.0 and stable native ABI 1 remain unchanged.

## GameMaster window route in Framework 0.5.3

Framework 0.5.3 adds the exclusive
`mp.management-gm-window-command.6ea5a0` entry route for the original GameMaster
window command. The matching Multiplayer plugin uses it for its original
Hire/Fire controls. The native catalog includes the exact entry and complete
function-body pins. Verify the pristine body before preparing its first hook,
then retain the authenticated entry, trampoline and owner for its full lifetime.

This is a native adapter capability, not a new Lua function or Routes protocol.
Use the matching Runtime and SDK; the stable plugin ABI and existing Lua APIs
remain unchanged.

## Save and task completion in Framework 0.5.4

A healthy [Recovery watch](step-recovery.md) can observe an ordinary Save without
adopting it. A real interruption still requires strict recovery admission before
its separate service can run.

During an admitted original simulation step, cancelled ToonPlan tasks can finish
through the original native Pump before the later full-step drain. The original
cancellation loop needs those completions to return. Live plans retain their
existing deferral and full-step ordering. Cancelled callbacks keep their original
effects, including the native special delivery branch, completion and orphan
cleanup; they are not replaced with no-ops.

Native mods can check the loaded Runtime's standard Windows
[version resource](../compatibility.md#runtime-versions-for-native-mods).
The stable ABI, Lua API and experimental draft versions remain unchanged.

Framework 0.5.4 also adds `MT2FW_MT2_SNAPSHOT_ARM_PERIODIC` with value **7**.
Its request layout is identical to `ARM`; existing operation values 1 through 6
remain unchanged. Existing consumers can continue using `ARM`. A consumer of the
new operation must require Runtime 0.5.4 or later; Step draft 9 availability alone
does not establish support for this additional operation.

Use `ARM_PERIODIC` only for an explicitly identified original Host periodic Save.
The matching Multiplayer protocol carries and validates that purpose separately
from a normal snapshot or reload. Framework retains it on the exact snapshot
certificate. A Client's native autosave notice binds only to a fresh periodic
certificate at or after its accepted local request, then completes once through
original `NotifyDoneSaving` after that certificate's successful shared release.
Normal `ARM`, an older capture, failed publication or a different nonce cannot
complete that notice. Framework does not create a Client writer or decrement its
zero local Save count.

The Client completion path requires Multiplayer Early Beta 27 or later, which
sends the explicit periodic purpose and forwards an automatic Client request to
the Host. Earlier Multiplayer builds use normal `ARM` and do not acquire this
Client completion path merely by updating the Runtime.

## Start with the matching header

The source header is
[`adapters/mt2/sdk/native_steps_draft.h`](https://github.com/DooDesch-Mods/MMORPGTycoon2-ModFramework/blob/main/adapters/mt2/sdk/native_steps_draft.h).
The developer download includes it at `mt2fw/sdk/native/mt2/native_steps_draft.h`
alongside its dependent native headers. Stable plugin declarations are under
`mt2fw/include/mt2fw/plugin.h`.

Read the [machine contract](../public/api/experimental/steps.json) for discovery.
Use the header as the authority for C declarations, struct sizes and calling
conventions. The query export is `MT2FW_MT2_QueryStepServiceDraft`; pass
`MT2FW_MT2_STEP_DRAFT_VERSION`, currently **9**, and exact sizes. The retained
type name `MT2FW_MT2_StepServiceDraftV1` does not mean query version 1.

Resolve this export from the already loaded Framework. Query with the borrowed
Host of the active supported native plugin, on its bound game thread. A missing
export, unsupported version, wrong owner or insufficient output size must stop
admission. Do not probe another loader or guess a compatible layout.

## Follow the retained lifecycle

| Method | Intended use |
| --- | --- |
| `bind` | Retain callbacks and arm initialization. Binding does not load a world or advance simulation. |
| `activate` | Enter the held session after real bootstrap, matching snapshot identity and peer agreement. |
| `read` | Copy the local receipt without running native work or repairing state. |
| `unbind` | Retire callbacks. Keep callback and user storage alive while the result is `BUSY`. |
| `preflight_exit` | Check admission for a fresh explicit exit. The check does not quit. |
| `snapshot` | Prepare, capture, complete and publish an ordered Save through separate acknowledged operations. |
| `exit_save` | Retain the matching Quit Save intent or terminal/reload retirement scope. It does not call original Quit. |

Callbacks must be synchronous and nonblocking. Preserve the exact owner,
provider token, session, thread, lease, world epoch, tick and invocation. A
borrowed `Next` continuation belongs to one invocation and permits one attempt.
Do not retain it for a later callback or retry it after an error or timeout.

The receipt distinguishes `not_attempted`, `confirmed` and `unknown`. An
accepted request or a returned callback alone is not proof of a confirmed world
change. Keep the original cause and inspect the retained result before deciding
what to do next.

At an idle outer boundary, Framework can prove an actual recovery interruption
while retaining the native hold. In the held service, `MT2FW_MT2_BUSY` is a
yield before command or native work on a typed interruption. It does not permit
replaying ordinary service work or borrowing `Next`.

## Keep Save completion separate from publication

A native writer can finish before companion files or peer acknowledgements are
complete. Do not report a successful shared Save until the required publication
is acknowledged. Do not start a second Save to repair an uncertain first one.

A failed session can service its local error window and a fresh explicit Leave
without borrowing `Next`, advancing simulation or clearing the failed snapshot.
Terminal retirement refuses an incomplete native async writer. A completed
capture owner can be retired through the admitted terminal path before the
original Quit queues its separate autosave.

Framework shutdown keeps the native services alive through plugin retirement.
Native callback storage remains retained while callbacks or unknown outcomes
still own it. No STL object, C++ exception or transferred ownership may cross the
C interface.

## Lua and other mods

Step draft 9 does not add raw pointers, seed control or scheduler control to Lua.
Existing local UI mods keep their normal APIs; the Step extension does not grant
them permission to change a shared world. Use game-thread callbacks for world
queries and cache their results for render callbacks.
