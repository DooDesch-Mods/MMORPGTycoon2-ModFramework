-- SPDX-License-Identifier: MIT
local pending, result
mt2.config.define("x", {type="number", default=0, label="Owned buildable X", min=-100000, max=100000})
mt2.config.define("z", {type="number", default=0, label="Owned buildable Z", min=-100000, max=100000})
mt2.ui.window("Placement", function()
    mt2.ui.text("Choose an owned clear position in settings before placing.")
    if mt2.world.status().loaded and mt2.ui.button("Queue scenery placement") then
        pending = mt2.actions.place("mt2:Scenery/circustent", mt2.config.get("x"), 0, mt2.config.get("z"))
        result = nil
    end
    if result then mt2.ui.text(result.status .. ": " .. (result.cause or "")) end
end)
mt2.events.on("action_completed", function(value)
    if pending and value.id == pending.id then
        result = value
        if value.status ~= "confirmed" then mt2.log.warn(value.cause or value.status) end
    end
end)
