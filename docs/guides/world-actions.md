---
title: Objects and world actions
---

# Objects and world actions

Read supported objects through handles. A handle's epoch identifies the active world; its UID is an exact string. Do not convert that string to a Lua number or retain a session handle across world changes.

```lua
mt2.ui.window("World Inspector", function()
    if mt2.world.status().loaded and mt2.ui.button("Read selection") then
        local selected = mt2.objects.selected()
        if selected then mt2.log.info("Selected UID: " .. selected.handle.uid) end
    end
end)
```

Declare `ui`, `world.read` and `objects.read`. The inspector reads explicitly rather than scanning every frame. Object lists are bounded to 128 results.

## Submit an action

World writes need `objects.write` and a `singleplayer` or `shared` manifest class. Shared writes are currently unavailable in multiplayer. Configure an owned, clear buildable position before placing anything.

```lua
local pending
-- Call from a user-triggered callback in a loaded world:
local function place_my_scenery(x, y, z)
    pending = mt2.actions.place("yourname.scenery:pyramid", x, y, z)
end
mt2.events.on("action_completed", function(result)
    if pending and result.id == pending.id then
        mt2.log.info(result.status)
        if result.cause and result.cause ~= "" then mt2.log.warn(result.cause) end
    end
end)
```

| Status | Meaning |
| --- | --- |
| `accepted` | Queued for the game thread; no success yet. |
| `confirmed` | The native operation and its readback confirmed success. |
| `rejected` | No confirmed operation; inspect the cause. |
| `unknown` | A native attempt may have occurred; inspect current state before any further action. |

Never automatically retry `unknown`, a timed-out action or an unacknowledged mutation. Requests have unique IDs and belong to their mod. Placement uses native terrain height; movement can settle to native ground height. Lua reload preserves already performed world changes.

Move and remove use the same status contract. Native object observers report supported original placement/move/removal as `object_changed`; these notifications do not promise every internal property change in the game. See [actions](../reference/functions/actions-place.md), [persistent references](../reference/functions/objects-persist.md) and [compatibility](../compatibility.md).
