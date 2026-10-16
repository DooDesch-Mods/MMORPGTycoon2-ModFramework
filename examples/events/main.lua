-- SPDX-License-Identifier: MIT
mt2.events.on("world_ready", function(event)
    mt2.log.info("World ready: " .. event.name)
end)
mt2.events.on("world_leave", function(event)
    mt2.log.info("Leaving: " .. event.name)
end)
