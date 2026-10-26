---
title: Experimental gameplay APIs
---

# Experimental gameplay APIs

Use Lua to build character editors, Dungeon tools and management mods. The gameplay
API uses named operations and typed values. Game addresses and native pointers stay
inside the MT2 adapter.

This is **gameplay protocol 1**, introduced with Lua API **1.2.0**. It requires the
matching Framework **0.3.0** runtime. These APIs are experimental. A compiled native
binding and a synthetic-host test do not establish that every game combination works.

Framework **0.4.0 / API 1.3.0** adds targeted queries, sparse terrain
samples and paired fee changes. These additions require Framework 0.4.0 or
later. Existing API 1.2 functions remain available.

## Find the operation

Use the [generated API reference](../reference/index.md) and
[machine contract](../public/api/experimental/gameplay.json). They are generated from
the same canonical contract as the Lua editor definitions and runtime wrappers.
`mt2.experimental.operations()` lists the operations in the installed runtime.

The [Gameplay API Explorer](https://github.com/DooDesch-Mods/MMORPGTycoon2-ModFramework/tree/main/examples/gameplay)
is a complete read-only mod. It does not require a new function key.

## Read from a game callback

Declare `world.read` for gameplay queries. Cache their results in `update` or
`world_ready`, then display the cached data from `render` or `ui.window`. Game queries
and actions cannot run from render callbacks.

```lua
local definitions = {}

mt2.events.on('world_ready', function()
    definitions = mt2.characters.list()
    for _, definition in ipairs(definitions) do
        mt2.log.info(definition.name)
    end
end)
```

Object and definition handles identify an exact native lifetime in one world epoch.
Keep UID strings as strings. Fetch new handles after a world change. Region and
subscriber handles use their own typed IDs; they cannot be substituted for object
handles. Paged or bounded queries report their limits and completion state.

## Change the world and check the result

Declare `objects.write` for gameplay actions. A mod that writes to the world must be
`singleplayer` or `shared` in its manifest. The `local` class cannot request these
writes. Existing APIs and native plugin ABI 1 remain available.

```lua
local pending

-- Call this from your mod's game-thread command handler with a current handle.
local function rename_character(handle, name)
    local accepted = mt2.characters.rename(handle, name)
    pending = accepted.id
end

mt2.events.on('action_completed', function(action)
    if action.id ~= pending then return end
    pending = nil
    if action.status == 'confirmed' then
        mt2.log.info('Character renamed.')
    else
        mt2.log.error(action.cause or action.status)
    end
end)
```

An action returns an ID and `accepted` when it enters the game-thread queue. The
adapter validates its owner and world again before execution. A disabled or replaced
mod instance cannot leave an old queued mutation behind.

| Result | Meaning |
| --- | --- |
| `accepted` | Queued; the game effect is not confirmed. |
| `confirmed` | Native invocation and the required effect readback succeeded. |
| `rejected` | Rejected before any native mutation attempt. |
| `unknown` | Native execution was attempted, but its complete result could not be established. |

Use `action_completed` or `mt2.actions.status(id)`. The native result is in `result`.
Keep unknown outcomes visible and inspect current state. **Never automatically retry
an unknown action.** Already performed world changes survive Lua reload.

## Native and session requirements

The adapter checks the supported executable, current game thread, native object
owners and code. It verifies known Framework hooks instead of bypassing them.
Actions also require an idle native world without an active save, edit or map task.
Inputs are checked before native invocation; readback and cleanup failures retain
their original cause.

Standalone mutation paths cannot bypass a Multiplayer session. Shared changes need
an acknowledged Host action provider and peer results. A manifest saying `shared`
does not establish Host authority or make an operation synchronized.

Some native game operations normalize values or affect related objects. Read each
function's contract before using it, particularly character removal, ability edits,
Dungeon instances, marketing and release lifecycle actions. Use copies of saves when
developing experimental mods.
