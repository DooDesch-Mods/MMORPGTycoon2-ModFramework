local message = "Hello, MT2!"
local world = "Waiting for a world"

mt2.config.define("visible", {type = "boolean", default = true, label = "Show greeting"})

mt2.events.on("world_ready", function(event)
    world = event.name
    mt2.log.info("World ready: " .. world)
end)

mt2.events.on("world_leave", function()
    world = "Waiting for a world"
end)

mt2.ui.window("Hello MT2", function()
    if mt2.config.get("visible") then
        mt2.ui.text(message)
        mt2.ui.text(world)
    end
end)
