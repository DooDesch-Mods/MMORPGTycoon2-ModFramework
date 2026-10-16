-- SPDX-License-Identifier: MIT
mt2.events.on("start", function()
    local starts = mt2.storage.get("starts", 0)
    mt2.storage.set("starts", starts + 1)
end)
mt2.events.on("world_ready", function()
    local visits = mt2.storage.get("visits", 0, "world")
    mt2.storage.set("visits", visits + 1, "world")
end)
