-- SPDX-License-Identifier: MIT
mt2.config.define("greeting", {type="string", default="Hello, MT2!", label="Greeting"})
mt2.ui.window("Settings", function()
    mt2.ui.text(mt2.config.get("greeting"))
    if mt2.ui.button("Use a new greeting") then mt2.config.set("greeting", "I made this.") end
end)
