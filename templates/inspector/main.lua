local selected = nil
local problem = nil

mt2.ui.window("World Inspector", function()
    if not mt2.world.status().loaded then
        mt2.ui.text("Load a world to inspect its objects.")
        return
    end
    if mt2.ui.button("Read selection") then
        local ok, result = pcall(mt2.objects.selected)
        if ok then selected, problem = result, nil else problem = result end
    end
    if problem then mt2.ui.text(problem) end
    if selected then
        mt2.ui.text("ID: " .. selected.handle.uid)
        mt2.ui.text(string.format("Position: %.2f, %.2f, %.2f", selected.x, selected.y, selected.z))
    end
end)

mt2.events.on("world_leave", function() selected = nil end)
