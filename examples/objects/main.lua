-- SPDX-License-Identifier: MIT
mt2.ui.window("Selected object", function()
    if not mt2.world.status().loaded then return end
    if mt2.ui.button("Read selection") then
        local selected = mt2.objects.selected()
        if selected then
            mt2.log.info("UID: " .. selected.handle.uid)
            local ok, id = pcall(mt2.objects.persist, selected.handle)
            if ok then mt2.storage.set("selected_building", id, "world")
            else mt2.log.warn("This selection cannot be persisted: " .. id) end
        end
    end
end)
mt2.events.on("world_ready", function()
    local id = mt2.storage.get("selected_building", nil, "world")
    if id then
        local ok, object = pcall(mt2.objects.resolve, id)
        if ok then mt2.log.info("Restored building: " .. object.handle.uid)
        else mt2.log.warn("Saved reference unavailable: " .. object) end
    end
end)
