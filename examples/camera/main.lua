-- SPDX-License-Identifier: MIT
mt2.ui.window("Camera", function()
    if not mt2.world.status().loaded then return end
    local view = mt2.camera.get()
    mt2.ui.text("Distance: " .. view.distance)
    if view.editable and mt2.ui.button("Focus here") then
        mt2.camera.focus(view.focus_x, view.focus_y, view.focus_z, 100)
    end
end)
