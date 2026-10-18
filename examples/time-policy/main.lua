-- SPDX-License-Identifier: MIT
local maximum_speed = 2
local message = "Choose a request after loading a world."
local last_warning
local ready = false

mt2.events.on("world_ready", function() ready = true end)
mt2.events.on("world_leave", function() ready = false end)

mt2.patches.add({
    id = "limit-speed",
    target = "api.time.set",
    mode = "modify"
}, {
    before = function(call)
        return {speed = math.min(call.speed, maximum_speed)}
    end
})

local function request_time(paused, speed)
    -- A denied authority check is expected here and must remain visible.
    local ok, result = pcall(mt2.time.set, paused, speed)
    if ok then
        message = "Confirmed API clock: speed " .. result.speed
        last_warning = nil
    else
        message = "Blocked: " .. tostring(result)
        if message ~= last_warning then
            mt2.log.warn(message)
            last_warning = message
        end
    end
end

mt2.ui.window("API time policy", function()
    mt2.ui.text("Framework time.set speed limit: " .. maximum_speed)
    mt2.ui.text("Native game controls keep their behavior.")
    local authority = mt2.patches.status().authority["api.time.set"]
    if not authority.allowed then mt2.ui.text("Blocked: " .. authority.reason) end
    if ready then
        if mt2.ui.button("Request speed 4 (limit 2)") then request_time(false, 4) end
        if mt2.ui.button("Request pause") then request_time(true, 1) end
    else
        mt2.ui.text("Load a world to send a time request.")
    end
    mt2.ui.text(message)
end)
