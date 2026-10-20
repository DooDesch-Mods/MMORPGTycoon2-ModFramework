-- SPDX-License-Identifier: MIT
local open, name, message = false, "Modder", "Welcome to MT2!"

mt2.events.on("render", function()
    local app = mt2.ui.app({id="main", title="Hello native Lua"})
    if app.open_requested then open = true end
    if not open then return end
    local result = mt2.ui.native({id="main", open=open, width=560, height=320,
        widgets={
            {id="intro", type="text", x=20, y=20, width=430, height=42,
                size=22, text="Your first native Lua app"},
            {id="preview", type="image", x=470, y=20, width=64, height=64,
                path="assets/icon.png"},
            {id="name", type="input", x=20, y=80, width=320, height=34,
                value=name, max_bytes=128},
            {id="greet", type="button", x=360, y=80, width=180, height=34,
                text="Say hello"},
            {id="message", type="text", x=20, y=140, width=520, height=80,
                text=message}
        }})
    if not result.ready then return end
    name = result.values.name or name
    for _, id in ipairs(result.clicked) do
        if id == "greet" then message = "Hello, " .. name .. "!" end
    end
    open = result.open
end)
