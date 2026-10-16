-- SPDX-License-Identifier: MIT
mt2.events.on("world_ready", function()
    local world = mt2.world.status()
    mt2.log.info(world.name .. " - epoch " .. world.epoch)
end)
