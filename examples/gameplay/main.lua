local operations = {}
local loaded = false

mt2.events.on('update', function()
    if not loaded then
        operations = mt2.experimental.operations()
        loaded = true
        mt2.log.info('Gameplay operations available: ' .. #operations)
    end
end)

mt2.ui.window('Gameplay API Explorer', function()
    mt2.ui.text('Experimental game APIs')
    mt2.ui.text('Use the generated SDK for parameters, handles and native requirements.')
    for _, operation in ipairs(operations) do
        mt2.ui.text(operation.name .. ' - ' .. operation.kind)
    end
end)
