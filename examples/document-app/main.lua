-- SPDX-License-Identifier: MIT
local open, search, selected = false, "", "Scenery"
local choices = {"Scenery", "Shops", "Monsters"}

mt2.events.on("render", function()
    local app = mt2.ui.app({id="main", title="Lua document browser"})
    if app.open_requested then open = true end
    if not open then return end
    local rows = {{type="input", id="search", hint="Filter names...", value=search}}
    for index, name in ipairs(choices) do
        if name:lower():find(search:lower(), 1, true) then
            rows[#rows+1] = {type="selectable", id="choice_"..index,
                title=name, subtitle="Example category", selected=selected==name}
        end
    end
    local result = mt2.ui.document({id="browser", title="Lua document browser",
        width=780, height=460, content={type="columns", children={
            {type="panel", id="list", width=280, children=rows},
            {type="panel", id="detail", children={
                {type="text", tone="heading", text=selected},
                {type="text", text="Select a category or filter the list."},
                {type="button", id="close", text="Close"}
            }}
        }}})
    search = result.values.search or search
    open = result.open
    for _, id in ipairs(result.clicked) do
        if id == "close" then open = false end
        for index, name in ipairs(choices) do
            if id == "choice_"..index then selected = name end
        end
    end
end)
