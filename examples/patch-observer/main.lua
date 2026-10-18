-- SPDX-License-Identifier: MIT
local entered, returned, available = 0, 0, 0
local last_invocation = "None"
local ready = false

mt2.events.on("world_ready", function() ready = true end)
mt2.events.on("world_leave", function() ready = false end)

mt2.patches.add({
    id = "count",
    target = "objects.create",
    mode = "observe"
}, {
    before = function(call)
        if not ready then return end
        entered = entered + 1
        last_invocation = call.invocation
    end,
    after = function(call)
        if not ready then return end
        if call.original_returned then returned = returned + 1 end
        if call.result_available then available = available + 1 end
    end
})

mt2.ui.window("Native create observer", function()
    if not ready then mt2.ui.text("Load a world, then create an object.") end
    mt2.ui.text("Native create calls entered: " .. entered)
    mt2.ui.text("Original calls returned: " .. returned)
    mt2.ui.text("Results available: " .. available)
    mt2.ui.text("Last invocation: " .. last_invocation)
    mt2.ui.text("Includes non-scenery creates.")
end)
