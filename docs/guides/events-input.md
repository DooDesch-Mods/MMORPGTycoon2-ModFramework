---
title: Events and input
---

# Events and input

Register event subscriptions once. They belong to your mod and are removed when the instance ends or is successfully replaced.

```lua
local token = mt2.events.on("world_ready", function(event)
    mt2.log.info("World: " .. event.name)
end)
-- Later, if needed: mt2.events.off(token)
```

The `start` event runs during preparation, before game-dependent APIs are available. Use `world_ready` to initialize work that needs a world. `world_leave` invalidates its session handles. A mod enabled into an existing world also receives `world_ready`.

## Bind a shortcut

Declare the `input` capability. Bind SDL scancode 66 for F9:

```lua
mt2.input.bind("show_message", 66, function(event)
    mt2.log.info("Pressed: " .. event.name)
end)
```

F10 belongs to Mods. Reserved or conflicting shortcuts report their owner. Input events go only to the owning mod, and mod shortcuts are suspended while the Mods menu is open.

## Keep callbacks bounded

Lua callbacks have instruction and memory budgets. An unhandled callback error disables its owner with the original cause. Dependency failures are shown for affected dependents. A failed replacement keeps the old instance, but world actions already performed cannot be undone by code reload.

Use update events for small scheduled work. Avoid world scans, file operations and repeated registrations on every frame. See [event payloads](../reference/events.md), [input.bind](../reference/functions/input-bind.md) and [ui.window](../reference/functions/ui-window.md).
