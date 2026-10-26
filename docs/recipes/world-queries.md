---
title: Read the part of the world you need
---

# Read the part of the world you need

These additions require **Framework 0.4.0 or later and Lua API 1.3.0**.
All examples below belong
in game-thread callbacks after `world_ready`. Cache their results for your UI.
The native APIs remain experimental; errors preserve their original cause.

## Region and terrain samples

```lua
local region = mt2.regions.at(100, 200)
if region then
    local first = mt2.terrain.sample(region, 0, 0)
    local values = mt2.terrain.samples(region, {
        {column=0, row=0},
        {column=1, row=0}
    })
    mt2.log.info('First raw height: ' .. first.height)
end
```

`regions.at(x, z)` searches the saved root world and returns `nil` outside the
map. It does not return a Dungeon Region. Reacquire the handle after world changes.

Terrain columns and rows are zero-based grid coordinates. Use
`terrain.heights(region, 0, 1)` for width, height, origin and cell size. A sample
contains `column`, `row`, flat `index` and raw `height`. It is not an interpolated
surface, world-space elevation conversion or a collision test.

Request at most 128 points. Results preserve input order, and duplicate points
are read once. Every coordinate is validated before sample memory is read. The
adapter reads the requested floats instead of copying the whole height grid and
rejects a changed grid identity. The complete read-only `examples/terrain-reader`
mod shows how to sample near the camera and refresh on request.

## One network object or project

```lua
local graph = mt2.network.snapshot()
if graph.nodes[1] then
    local node = mt2.network.node(graph.nodes[1].handle)
    mt2.log.info('Server type: ' .. node.type)
end
if graph.pipes[1] then
    local pipe = mt2.network.pipe(graph.pipes[1].handle)
    mt2.log.info('Pipe type: ' .. pipe.type)
end
local catalogue = mt2.projects.catalogue()
if catalogue[1] then
    local project = mt2.projects.get(catalogue[1].source)
    mt2.log.info('Can choose project: ' .. tostring(project.can_choose))
end
```

These functions return the existing typed node, pipe and project records. They
validate current ownership and reject stale or wrong-type handles. Network
getters validate the native graph and reciprocal connections; they do not turn
graph traversal into a constant-time operation. Refresh on demand, not per frame.
Keep exact UID strings intact. Project sources come from the current catalogue,
not a persisted address or an invented table.

## One building service

```lua
local services = mt2.management.services(building_handle)
if services[1] then
    local service = mt2.management.service(building_handle, services[1].handle)
    mt2.log.info('Service price: ' .. service.price)
end
```

Pass the owning building as well as the service handle. The adapter checks that
the service still belongs to that building and returns its current configuration.

## Change both fees and inspect completion

```lua
local pending

-- Invoke from your game-thread command handler, in a singleplayer world.
local function update_fees()
    local action = mt2.management.set_fees(41, 11)
    pending = action.id
end

mt2.events.on('action_completed', function(action)
    if action.id ~= pending then return end
    pending = nil
    if action.status == 'confirmed' then
        mt2.log.info('Both fees updated.')
    else
        mt2.log.error(action.cause or action.status)
    end
end)
```

Declare `objects.write`. Both inputs are validated before either original native
setter runs. The action checks both prices and the complete current financial
ledger afterward. It uses the two original Business setters used by the game's
Overview command, without opening or editing a native UI window.

One queued action is not a rollback transaction. If execution fails after the
first setter, the outcome is unknown. Read `management.fees()` to inspect current
state; never automatically retry the action. Existing individual setters remain
available. This addition grants no new Lua multiplayer mutation authority.
