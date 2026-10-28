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
