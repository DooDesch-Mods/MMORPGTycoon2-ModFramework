-- SPDX-License-Identifier: MIT
mt2.input.bind("show_message", 66, function(event)
    mt2.log.info("My F9 shortcut: " .. event.name)
end)
