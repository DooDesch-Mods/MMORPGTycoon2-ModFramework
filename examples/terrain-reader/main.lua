local message = 'Load a world to inspect its terrain.'
local samples = {}
local refresh = false

local function read_terrain()
    local camera = mt2.camera.get()
    local region = mt2.regions.at(camera.focus_x, camera.focus_z)
    if not region then
        samples = {}
        message = 'The camera is outside the root map.'
        return
    end
    local grid = mt2.terrain.heights(region, 0, 1)
    local column = math.max(0, math.min(grid.width - 1,
        math.floor((camera.focus_x - grid.origin[1]) / grid.cell_size)))
    local row = math.max(0, math.min(grid.height - 1,
        math.floor((camera.focus_z - grid.origin[3]) / grid.cell_size)))
    samples = mt2.terrain.samples(region, {
        {column = column, row = row},
        {column = math.min(column + 1, grid.width - 1), row = row}
    })
    message = 'Region ' .. region.id .. ' - raw height-grid values'
end

mt2.events.on('world_ready', function()
    refresh = true
end)
mt2.events.on('world_leave', function()
    refresh = false
    samples = {}
    message = 'Load a world to inspect its terrain.'
end)
mt2.events.on('update', function()
    if not refresh or not mt2.world.status().loaded then return end
    refresh = false
    read_terrain()
end)
mt2.ui.window('Terrain Reader', function()
    mt2.ui.text(message)
    for _, sample in ipairs(samples) do
        mt2.ui.text(string.format('Column %d, row %d: %.2f',
            sample.column, sample.row, sample.height))
    end
    if mt2.ui.button('Read at camera') then refresh = true end
end)
