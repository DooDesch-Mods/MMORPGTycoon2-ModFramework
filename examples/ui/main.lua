-- SPDX-License-Identifier: MIT
local enabled, amount = true, 5
mt2.ui.window("My mod", function()
    mt2.ui.text("Hello, MT2!")
    enabled = mt2.ui.checkbox("Enabled", enabled)
    amount = mt2.ui.number("Amount", amount, 1, 10)
    if mt2.ui.button("Say hello") then mt2.log.info("Hello from my mod") end
end)
