-- SPDX-License-Identifier: MIT
mt2.ui.window("Time", function()
    if not mt2.world.status().loaded then return end
    local clock = mt2.time.get()
    mt2.ui.text("Day: " .. clock.day)
    if mt2.ui.button("Pause") then mt2.time.set(true, 1) end
    if mt2.ui.button("Resume") then mt2.time.set(false, 1) end
end)
